
#include "dvfsTSP.h"
#include "powermodel.h"
#include <iomanip>
#include <iostream>
using namespace std;

DVFSTSP::DVFSTSP(ThermalModel *thermalModel, const PerformanceCounters *performanceCounters, int coreRows, int coreColumns, int minFrequency, int maxFrequency, int frequencyStepSize)
   : thermalModel(thermalModel), performanceCounters(performanceCounters), coreRows(coreRows), coreColumns(coreColumns), minFrequency(minFrequency), maxFrequency(maxFrequency), frequencyStepSize(frequencyStepSize){
  
}

std::vector<int> DVFSTSP::getFrequencies(const std::vector<int> &oldFrequencies, const std::vector<bool> &activeCores) {
   std::vector<int> frequencies(coreRows * coreColumns);


   float tsp = thermalModel->tsp(activeCores);


   for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {
       if (activeCores.at(coreCounter)) {
           float power = performanceCounters->getPowerOfCore(coreCounter);
           float temperature = performanceCounters->getTemperatureOfCore(coreCounter);
           int frequency = oldFrequencies.at(coreCounter);
           float utilization = performanceCounters->getUtilizationOfCore(coreCounter);


           cout << "[Scheduler][DVFSTSP]: Core " << setw(2) << coreCounter << ":";
           cout << " P=" << fixed << setprecision(3) << power << " W";
           cout << " (budget: " << fixed << setprecision(3) << tsp << " W)";
           cout << " f=" << frequency << " MHz";
           cout << " T=" << fixed << setprecision(1) << temperature << " °C";
           cout << " utilization=" << fixed << setprecision(3) << utilization << endl;






           int expectedGoodFrequency = PowerModel::getExpectedGoodFrequency(frequency, power, tsp, minFrequency, maxFrequency, frequencyStepSize);
           frequencies.at(coreCounter) = expectedGoodFrequency;


            // added by anu


           float RelNUCACPI  = performanceCounters->getRelNUCACPIOfCore(coreCounter);
           float IPS         = performanceCounters->getIPSOfCore(coreCounter);
           float cpi_total   = performanceCounters->getCPIOfCore(coreCounter);
           float peak_temperature = performanceCounters->getPeakTemperature();


           cout << " IPS=" << fixed << setprecision(3) << IPS ;
           cout << " RelNUCACPI=" << fixed << setprecision(3) << RelNUCACPI ;
           cout << " hotspot_peak_temperature=" << fixed << setprecision(3) << peak_temperature ;
           cout << " cpi-total=" << fixed << setprecision(3) <<cpi_total << endl;






       } else {
           frequencies.at(coreCounter) = minFrequency;
       }
   }




// added by anu
   std::ofstream outputFilep("insta_power_used.txt");
   std::ofstream outputFilePw("periodic_power_used.txt",fstream::app);


   for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {


      outputFilep << std::fixed << std::setprecision(4) << performanceCounters->getPowerOfCore(coreCounter) << "\t";
      outputFilePw << std::fixed << std::setprecision(4) << performanceCounters->getPowerOfCore(coreCounter) << "\t";
   }


   outputFilep << "\n" ;
   outputFilePw << "\n";
   outputFilep.close();
   outputFilePw.close();


// added by anu
   std::ofstream outputFilePA("insta_powerAllocatedTSP.txt");
   std::ofstream outputFilePt("periodic_powerAllocatedTSP.txt",fstream::app);


   for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {


      outputFilePA << std::fixed << std::setprecision(4) << tsp << "\t";
      outputFilePt<< std::fixed << std::setprecision(4) << tsp << "\t";
   }


   outputFilePA << "\n" ;
   outputFilePt << "\n";
   outputFilePA.close();
   outputFilePt.close();




// added by anu
   std::ofstream outputFileC("instacpi_total.txt");
   std::ofstream outputFileCP("periodiccpi_total.txt",fstream::app);


   for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {


      outputFileC << std::fixed << std::setprecision(4) << performanceCounters->getCPIOfCore(coreCounter) << "\t";
      outputFileCP << std::fixed << std::setprecision(4) << performanceCounters->getCPIOfCore(coreCounter) << "\t";
   }


   outputFileC << "\n" ;
   outputFileCP << "\n";
   outputFileC.close();
   outputFileCP.close();


// added by anu
   std::ofstream outputFileI("instaIPS.txt");
   std::ofstream outputFileIP("periodicIPS.txt",fstream::app);


   for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {


      outputFileI << std::fixed << std::setprecision(3) << performanceCounters->getIPSOfCore(coreCounter) << "\t";
      outputFileIP << std::fixed << std::setprecision(3) << performanceCounters->getIPSOfCore(coreCounter) << "\t";
   }


   outputFileI << "\n" ;
   outputFileIP << "\n";
   outputFileI.close();
   outputFileIP.close();


// added by anu
   std::ofstream outputFileR("instaRelNUCACPI.txt");
   std::ofstream outputFileRP("periodicRelNUCACPI.txt",fstream::app);


   for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {


      outputFileR << std::fixed << std::setprecision(3) << performanceCounters->getRelNUCACPIOfCore(coreCounter) << "\t";
      outputFileRP << std::fixed << std::setprecision(3) << performanceCounters->getRelNUCACPIOfCore(coreCounter) << "\t";
   }


   outputFileR << "\n" ;
   outputFileRP << "\n";
   outputFileR.close();
   outputFileRP.close();


// added by anu
   std::ofstream outputFile("instaUtlization.txt");
   std::ofstream outputFileP("periodicUtlization.txt",fstream::app);


   for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {


      outputFile << std::fixed << std::setprecision(3) << performanceCounters->getUtilizationOfCore(coreCounter) << "\t";
      outputFileP << std::fixed << std::setprecision(3) << performanceCounters->getUtilizationOfCore(coreCounter) << "\t";
   }


   outputFile << "\n" ;
   outputFileP << "\n";
   outputFile.close();
   outputFileP.close();


// added by anu
   std::ofstream outputFileT("insta_anu_Temperature.txt");
   std::ofstream outputFileTP("periodic_anu_Temperature.txt",fstream::app);


   for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {


      outputFileT << std::fixed << std::setprecision(3) << performanceCounters->getTemperatureOfCore(coreCounter) << "\t";
      outputFileTP << std::fixed << std::setprecision(3) << performanceCounters->getTemperatureOfCore(coreCounter) << "\t";
   }


   outputFileT << "\n" ;
   outputFileTP << "\n";
   outputFileT.close();
   outputFileTP.close();


// added by anu
   std::ofstream outputFilePT("insta_peak_Temperature.txt");
   std::ofstream outputFilePTP("periodic_peak_Temperature.txt",fstream::app);


   for (unsigned int coreCounter = 0; coreCounter < coreRows * coreColumns; coreCounter++) {


      outputFilePT << std::fixed << std::setprecision(3) << performanceCounters->getPeakTemperature() << "\t";
      outputFilePTP << std::fixed << std::setprecision(3) << performanceCounters->getPeakTemperature()<< "\t";
   }


   outputFilePT << "\n" ;
   outputFilePTP << "\n";
   outputFilePT.close();
   outputFilePTP.close();


   return frequencies;
}








