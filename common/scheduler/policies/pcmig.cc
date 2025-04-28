#include "pcmig.h"
#include <iomanip>
#include <limits>
#include <tuple>
#include <fstream> // NEW include
#include "powermodel.h"

using namespace std;

PCMig::PCMig(ThermalModel *thermalModel, PerformanceCounters *performanceCounters, int coreRows, int coreColumns, int minFrequency, int maxFrequency, int frequencyStepSize, float delta)
     : thermalModel(thermalModel), performanceCounters(performanceCounters), coreRows(coreRows), coreColumns(coreColumns),
	 minFrequency(minFrequency), maxFrequency(maxFrequency), frequencyStepSize(frequencyStepSize), delta(delta) {

    // get core AMD info
    for (int y = 0; y < coreColumns; y++) {
		for (int x = 0; x < coreRows; x++) {
			float amd = getCoreAMD(y, x);
			amds.push_back(amd);
			uniqueAMDs.insert(amd);
			threadStates.push_back(ThreadState::IDLE);
			powerBudgets.push_back(thermalModel->getInactivePower());
		}
	}

    // Open CSV file
    logFile.open("logs.csv", std::ios::out | std::ios::app); // append mode
    if (!logFile.is_open()) {
        cerr << "Error opening logs.csv for writing!" << endl;
        exit(1);
    }
    headerWritten = false;
}

PCMig::~PCMig() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

int PCMig::manhattanDistance(int y1, int x1, int y2, int x2) {
	int dy = y1 - y2;
	int dx = x1 - x2;
	return abs(dy) + abs(dx);
}

float PCMig::getCoreAMD(int coreY, int coreX) {
	int md_sum = 0;
	for (unsigned int y = 0; y < coreColumns; y++) {
		for (unsigned int x = 0; x < coreRows; x++) {
			md_sum += manhattanDistance(coreY, coreX, y, x);
		}
	}
	return (float)md_sum / coreColumns / coreRows;
}

// The rest (getMappingCandidates and map) are unchanged...

void PCMig::updatePowerBudgets(const std::vector<int> &oldFrequencies) {
	bool recalculateComputeBound = false;
	for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {
		double utilization = performanceCounters->getUtilizationOfCore(coreCounter);
		double power = utilization > 0 ? performanceCounters->getPowerOfCore(coreCounter) : 0;
		int frequency = oldFrequencies.at(coreCounter);
		bool atMaximumFrequency = (frequency == maxFrequency);
		switch (threadStates.at(coreCounter)) {
			case ThreadState::IDLE:
				if (utilization > 0) {
					cout << "[Scheduler][PCMig] Core " << coreCounter << " switches to state COMPUTE" << endl;
					threadStates.at(coreCounter) = ThreadState::COMPUTE;
					powerBudgets.at(coreCounter) = -1;
					recalculateComputeBound = true;
				}
				break;
			case ThreadState::COMPUTE:
				if (utilization == 0) {
					cout << "[Scheduler][PCMig] Core " << coreCounter << " switches to state IDLE due to low utilization" << endl;
					threadStates.at(coreCounter) = ThreadState::IDLE;
					powerBudgets.at(coreCounter) = thermalModel->getInactivePower();
					recalculateComputeBound = true;
				} else if (atMaximumFrequency && (power < powerBudgets.at(coreCounter) - delta)) {
					cout << "[Scheduler][PCMig] Core " << coreCounter << " switches to state MEMORY due to low power consumption" << endl;
					threadStates.at(coreCounter) = ThreadState::MEMORY;
					powerBudgets.at(coreCounter) = power + delta;
					recalculateComputeBound = true;
				}
				break;
			case ThreadState::MEMORY:
				if (utilization == 0) {
					cout << "[Scheduler][PCMig] Core " << coreCounter << " switches to state IDLE due to low utilization" << endl;
					threadStates.at(coreCounter) = ThreadState::IDLE;
					powerBudgets.at(coreCounter) = thermalModel->getInactivePower();
					recalculateComputeBound = true;
				} else if (!atMaximumFrequency || (power > powerBudgets.at(coreCounter))) {
					cout << "[Scheduler][PCMig] Core " << coreCounter << " switches to state COMPUTE due to high power consumption" << endl;
					threadStates.at(coreCounter) = ThreadState::COMPUTE;
					powerBudgets.at(coreCounter) = -1;
					recalculateComputeBound = true;
				} else if (power < powerBudgets.at(coreCounter) - delta) {
					powerBudgets.at(coreCounter) = power + delta;
					recalculateComputeBound = true;
				}
				break;
			default:
				cout << "PCMig::updatePowerBudgets: unknown thread state encountered" << endl;
				exit(1);
		}
	}

	if (recalculateComputeBound) {
		std::vector<bool> unrestrictedCores(coreRows * coreColumns);
		for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {
			unrestrictedCores.at(coreCounter) = (threadStates.at(coreCounter) == ThreadState::COMPUTE);
		}
		float uniformPerCorePowerBudget = thermalModel->tsp(unrestrictedCores);
		for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {
			if (unrestrictedCores.at(coreCounter)) {
				powerBudgets.at(coreCounter) = uniformPerCorePowerBudget;
			}
		}
	}
}

std::vector<int> PCMig::getFrequencies(const std::vector<int> &oldFrequencies, const std::vector<bool> &activeCores) {
	updatePowerBudgets(oldFrequencies);

	std::vector<int> frequencies(coreRows * coreColumns);

	for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {
		if (activeCores.at(coreCounter)) {
			float powerBudget = powerBudgets.at(coreCounter);
			float power = performanceCounters->getPowerOfCore(coreCounter);
			float temperature = performanceCounters->getTemperatureOfCore(coreCounter);
			int frequency = oldFrequencies.at(coreCounter);
			float utilization = performanceCounters->getUtilizationOfCore(coreCounter);

			float RelNUCACPI  = performanceCounters->getRelNUCACPIOfCore(coreCounter);
           	float IPS         = performanceCounters->getIPSOfCore(coreCounter);
           	float cpi_total   = performanceCounters->getCPIOfCore(coreCounter);
           	float peak_temperature = performanceCounters->getPeakTemperature();

            // Identify thread state
            string threadStateStr;
            switch (threadStates.at(coreCounter)) {
                case ThreadState::IDLE: threadStateStr = "IDLE"; break;
                case ThreadState::COMPUTE: threadStateStr = "COMPUTE"; break;
                case ThreadState::MEMORY: threadStateStr = "MEMORY"; break;
                default: threadStateStr = "UNKNOWN"; break;
            }

			// Print to console
			cout << "[Scheduler][PCMig]: Core " << setw(2) << coreCounter << " [" << threadStateStr << "] "
			     << ": P=" << fixed << setprecision(4) << power << " W"
			     << " (budget: " << fixed << setprecision(4) << powerBudget << " W)"
			     << " f=" << frequency << " MHz"
			     << " T=" << fixed << setprecision(1) << temperature << " °C"
			     << " utilization=" << fixed << setprecision(4) << utilization << endl;

			cout << " IPS=" << fixed << setprecision(3) << IPS 
			     << " RelNUCACPI=" << fixed << setprecision(3) << RelNUCACPI
			     << " hotspot_peak_temperature=" << fixed << setprecision(3) << peak_temperature
			     << " cpi-total=" << fixed << setprecision(3) << cpi_total << endl;

            // Write CSV header once
            if (!headerWritten) {
                logFile << "core_id,thread_state,power,power_budget,frequency,temperature,utilization,IPS,RelNUCACPI,hotspot_peak_temperature,cpi_total" << endl;
                headerWritten = true;
            }

            // Write to CSV
            logFile << coreCounter << "," 
                    << threadStateStr << ","
                    << fixed << setprecision(4) << power << ","
                    << fixed << setprecision(4) << powerBudget << ","
                    << frequency << ","
                    << fixed << setprecision(1) << temperature << ","
                    << fixed << setprecision(4) << utilization << ","
                    << fixed << setprecision(3) << IPS << ","
                    << fixed << setprecision(3) << RelNUCACPI << ","
                    << fixed << setprecision(3) << peak_temperature << ","
                    << fixed << setprecision(3) << cpi_total
                    << endl;

			int expectedGoodFrequency = PowerModel::getExpectedGoodFrequency(frequency, power, powerBudget, minFrequency, maxFrequency, frequencyStepSize);
			frequencies.at(coreCounter) = expectedGoodFrequency;
		} else {
			frequencies.at(coreCounter) = minFrequency;
		}
	}

	return frequencies;
}


/** getMappingCandidates
 * Get all near-Pareto-optimal mappings considered in PCMig.
 * Return a vector of tuples (max. AMD, power budget, used cores)
 */
vector<tuple<float, float, vector<int>>> PCMig::getMappingCandidates(int taskCoreRequirement, const vector<bool> &availableCores, const vector<bool> &activeCores) {
	// get the candidates
	vector<tuple<float, float, vector<int>>> candidates;
	for (const float &amdMax : uniqueAMDs) {
		// get the candidate for the given AMD_max

		vector<int> availableCoresAMD;
		for (unsigned int i = 0; i < coreRows * coreColumns; i++) {
			if (availableCores.at(i) && (amds.at(i) <= amdMax)) {
				availableCoresAMD.push_back (i);
			}
		}

		if ((int)availableCoresAMD.size() >= taskCoreRequirement) {
			vector<int> selectedCores;

			vector<bool> activeCoresCandidate = activeCores;

			float mappingTSP = 0;
			float maxUsedAMD = 0;
			while ((int)selectedCores.size() < taskCoreRequirement) {
				// greedily select one core
				vector<double> tsps = thermalModel->tspForManyCandidates(activeCoresCandidate, availableCoresAMD);
				float bestTSP = 0;
				int bestIndex = 0;
				for (unsigned int i = 0; i < tsps.size(); i++) {
					if (tsps.at(i) > bestTSP) {
						bestTSP = tsps.at(i);
						bestIndex = i;
					}
				}

				activeCoresCandidate[availableCoresAMD.at(bestIndex)] = true;
				selectedCores.push_back(availableCoresAMD.at(bestIndex));

				mappingTSP = bestTSP;
				maxUsedAMD = max(maxUsedAMD, amds.at(availableCoresAMD.at(bestIndex)));

				availableCoresAMD.erase(availableCoresAMD.begin() + bestIndex);
			}

			if (maxUsedAMD == amdMax) {
				// add the mapping to the list of mappings
				tuple<float, float, vector<int>> mapping(amdMax, mappingTSP, selectedCores);
				candidates.push_back(mapping);
			}
		}
	}

	return candidates;
}

/** map
    This function performs patterning
*/
std::vector<int> PCMig::map(String taskName, int taskCoreRequirement, const vector<bool> &availableCores, const vector<bool> &activeCores) {
	// get the mapping candidates
	vector<tuple<float, float, vector<int>>> mappingCandidates = getMappingCandidates(taskCoreRequirement, availableCores, activeCores);

	// find the best mapping
	int bestMappingNb = 0;
	if (mappingCandidates.size() == 0) {
        vector<int> empty;
		return empty;
	} else if (mappingCandidates.size() == 1) {
		bestMappingNb = 0;
	} else {
		float minAmd = get<0>(mappingCandidates.front());
		float minPowerBudget = get<1>(mappingCandidates.front());
		float deltaAmd = get<0>(mappingCandidates.back()) - minAmd;
		float deltaPowerBudget = get<1>(mappingCandidates.back()) - minPowerBudget;

		float alpha = deltaPowerBudget / deltaAmd;

		float bestRating = numeric_limits<float>::lowest();

		for (unsigned int mappingNb = 0; mappingNb < mappingCandidates.size(); mappingNb++) {
			float amdMax = get<0>(mappingCandidates.at(mappingNb));
			float powerBudget = get<1>(mappingCandidates.at(mappingNb));
			float rating = (powerBudget - minPowerBudget) - alpha * (amdMax - minAmd);
			vector<int> cores = get<2>(mappingCandidates.at(mappingNb));

			cout << "Mapping Candidate: rating; " << setprecision(3) << rating << ", power budget: " << setprecision(3) << powerBudget << " W, max. AMD: " << setprecision(3) << amdMax;
			cout << ", used cores:";
			for (unsigned int i = 0; i < cores.size(); i++) {
				cout << " " << cores.at(i);
			}
			cout << endl;

			if (rating > bestRating) {
				bestRating = rating;
				bestMappingNb = mappingNb;
			}
		}
	}

	// return the cores
	return get<2>(mappingCandidates.at(bestMappingNb));
}

void PCMig::updatePowerBudgets(const std::vector<int> &oldFrequencies) {
	bool recalculateComputeBound = false;
	for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {
		double utilization = performanceCounters->getUtilizationOfCore(coreCounter);
		double power = utilization > 0 ? performanceCounters->getPowerOfCore(coreCounter) : 0;
		int frequency = oldFrequencies.at(coreCounter);
		bool atMaximumFrequency = (frequency == maxFrequency);
		switch (threadStates.at(coreCounter)) {
			case ThreadState::IDLE:
				if (utilization > 0) {
					cout << "[Scheduler][PCMig] Core " << coreCounter << " switches to state COMPUTE" << endl;
					threadStates.at(coreCounter) = ThreadState::COMPUTE;
					powerBudgets.at(coreCounter) = -1;
					recalculateComputeBound = true;
				}
				break;
			case ThreadState::COMPUTE:
				if (utilization == 0) {
					cout << "[Scheduler][PCMig] Core " << coreCounter << " switches to state IDLE due to low utilization" << endl;
					threadStates.at(coreCounter) = ThreadState::IDLE;
					powerBudgets.at(coreCounter) = thermalModel->getInactivePower();
					recalculateComputeBound = true;
				} else if (atMaximumFrequency && (power < powerBudgets.at(coreCounter) - delta)) {
					cout << "[Scheduler][PCMig] Core " << coreCounter << " switches to state MEMORY due to low power consumption" << endl;
					threadStates.at(coreCounter) = ThreadState::MEMORY;
					powerBudgets.at(coreCounter) = power + delta;
					recalculateComputeBound = true;
				}
				break;
			case ThreadState::MEMORY:
				if (utilization == 0) {
					cout << "[Scheduler][PCMig] Core " << coreCounter << " switches to state IDLE due to low utilization" << endl;
					threadStates.at(coreCounter) = ThreadState::IDLE;
					powerBudgets.at(coreCounter) = thermalModel->getInactivePower();
					recalculateComputeBound = true;
				} else if (!atMaximumFrequency || (power > powerBudgets.at(coreCounter))) {
					cout << "[Scheduler][PCMig] Core " << coreCounter << " switches to state COMPUTE due to high power consumption" << endl;
					threadStates.at(coreCounter) = ThreadState::COMPUTE;
					powerBudgets.at(coreCounter) = -1;
					recalculateComputeBound = true;
				} else if (power < powerBudgets.at(coreCounter) - delta) {
					powerBudgets.at(coreCounter) = power + delta;
					recalculateComputeBound = true;
				}
				break;
			default:
				cout << "PCMig::updatePowerBudgets: unknown thread state encountered" << endl;
				exit(1);
		}
	}

	if (recalculateComputeBound) {
		std::vector<bool> unrestrictedCores(coreRows * coreColumns);
		for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {
			unrestrictedCores.at(coreCounter) = (threadStates.at(coreCounter) == ThreadState::COMPUTE);
		}
		float uniformPerCorePowerBudget = thermalModel->tsp(unrestrictedCores);
		for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {
			if (unrestrictedCores.at(coreCounter)) {
				powerBudgets.at(coreCounter) = uniformPerCorePowerBudget;
			}
		}
	}
}

std::vector<int> PCMig::getFrequencies(const std::vector<int> &oldFrequencies, const std::vector<bool> &activeCores) {
	updatePowerBudgets(oldFrequencies);

	std::vector<int> frequencies(coreRows * coreColumns);

	for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {
		if (activeCores.at(coreCounter)) {
			float powerBudget = powerBudgets.at(coreCounter);
			float power = performanceCounters->getPowerOfCore(coreCounter);
			float temperature = performanceCounters->getTemperatureOfCore(coreCounter);
			int frequency = oldFrequencies.at(coreCounter);
			float utilization = performanceCounters->getUtilizationOfCore(coreCounter);

			cout << "[Scheduler][PCMig]: Core " << setw(2) << coreCounter << " ";
			switch (threadStates.at(coreCounter)) {
				case ThreadState::IDLE:
					cout << "[IDLE]   ";
					break;
				case ThreadState::COMPUTE:
					cout << "[COMPUTE]";
					break;
				case ThreadState::MEMORY:
					cout << "[MEMORY] ";
					break;
				default:
					cout << "[???????]";
					break;
			}
			cout << ": P=" << fixed << setprecision(4) << power << " W";
			cout << " (budget: " << fixed << setprecision(4) << powerBudget << " W)";
			cout << " f=" << frequency << " MHz";
			cout << " T=" << fixed << setprecision(1) << temperature << " °C";
			cout << " utilization=" << fixed << setprecision(4) << utilization << endl;

			float RelNUCACPI  = performanceCounters->getRelNUCACPIOfCore(coreCounter);
           	float IPS         = performanceCounters->getIPSOfCore(coreCounter);
           	float cpi_total   = performanceCounters->getCPIOfCore(coreCounter);
           	float peak_temperature = performanceCounters->getPeakTemperature();

			cout << " IPS=" << fixed << setprecision(3) << IPS ;
           	cout << " RelNUCACPI=" << fixed << setprecision(3) << RelNUCACPI ;
           	cout << " hotspot_peak_temperature=" << fixed << setprecision(3) << peak_temperature ;
           	cout << " cpi-total=" << fixed << setprecision(3) <<cpi_total << endl;


			int expectedGoodFrequency = PowerModel::getExpectedGoodFrequency(frequency, power, powerBudget, minFrequency, maxFrequency, frequencyStepSize);
			frequencies.at(coreCounter) = expectedGoodFrequency;
		} else {
			frequencies.at(coreCounter) = minFrequency;
		}
	}

	return frequencies;
}
