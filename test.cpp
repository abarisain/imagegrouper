#include <iostream>
#include <fstream>
#include <pHash.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include "FileHash.h"

#define SIMILARITY_THRESHOLD 0.2f

namespace bf=boost::filesystem;

static const char* validExtensions[] = {".jpeg", ".jpg", NULL};

//Checks if a string equals to one in the provided null-terminated array
bool isStringInArray(const std::string &s,
                     const char** array) {  
   while(*array) {
      if(boost::iequals(s, *array))
         return true;
      array++;
   }
   return false;
}

//Returns true if the path extension is in the list
bool checkPathExtension(const bf::path &file) { 
   return isStringInArray(file.extension().string(), validExtensions);
}

//Returns true on success, false on error
bool listImages(const bf::path &dir, //Root directory
                std::vector<FileHash> &files) {
   if(!exists(dir))
      return false; 
   bf::directory_iterator endIt; //Default dir_iter constructor yields after the end
   for(bf::directory_iterator it(dir); it != endIt; ++it) {
      if(bf::is_directory(it->status())) {
         listImages(it->path(), files);
      } else {
         if(checkPathExtension(it->path())) { 
            files.push_back(FileHash(it->path().string()));
         }
      }
   }
   return true;
}

void printProgressBar(int percent) {
   std::string bar;

   for(int i = 0; i < 50; i++){
      if( i <= (percent/2)){
         bar.replace(i,1,"="); 
      } else {
         bar.replace(i,1," ");
      }
   }
   std::cout<< "\r" "[" << bar << "] ";
   std::cout.width( 3 );
   std::cout<< percent << "%     " << std::flush;
}

void printUsageAndDie() {
   std::cout << " usage : duplicate_scanner [-a] algo threshold target_dir" << std::endl;
   std::cout << " options" << std::endl;
   std::cout << "        -a : Used with algo=all, makes threshold satisfaction mandatory for";
   std::cout << " only one hash instead of an average. More false positives,";
   std::cout << " but might miss a lot of similar images." << std::endl;
   std::cout << "      algo : Algorithm to use (dct, mh, radish, all -not recommended-)" << std::endl;
   std::cout << " threshold : Minimum similarity before a picture matches (%), recommended : 80" << std::endl;
   exit(0);
}

int main(int argc, char** argv) {
   if(argc < 4) {
      printUsageAndDie();
   }

   FileHashGrouper fileHashGrouper;
   bool enableHtmlOutput = false;
   char* targetFolder;
   char* algoArg;
   char* thresholdArg;
   float threshold = -1.0f;
   HashAlgorithm selectedAlgorithm = DCT; 

   #ifdef DEBUG
   std::cout << "Running in debug mode" << std::endl;
   #endif

   if(*argv[1] == '-') {
      if(argc < 5)
         printUsageAndDie();
      targetFolder = argv[4];
      algoArg = argv[2];
      thresholdArg = argv[3];
      if(argv[1][1] == 'a') {
         #ifndef DEBUG
         fileHashGrouper.thresholdRequiredForAll = false;
         std::cout << "Threshold will be required to be satisfied for only ONE hash." << std::endl;
         #else
         std::cout << "Threshold behaviour argument ignored because of debug mode." << std::endl;
         #endif
      }
   } else {
      targetFolder = argv[3];
      algoArg = argv[1];
      thresholdArg = argv[2];
   }

   enableHtmlOutput = true;
   std::cout << "HTML Output enabled." << std::endl;

   std::string tmpSelectedAlgorithm(algoArg);
   boost::algorithm::to_lower(tmpSelectedAlgorithm);
   if(boost::iequals(tmpSelectedAlgorithm, "dct"))
      selectedAlgorithm = DCT; 
   if(boost::iequals(tmpSelectedAlgorithm, "mh"))
      selectedAlgorithm = MH;
   if(boost::iequals(tmpSelectedAlgorithm, "radish"))
      selectedAlgorithm = RADISH;
   if(boost::iequals(tmpSelectedAlgorithm, "all"))
      selectedAlgorithm = ALL;
   fileHashGrouper.algorithm = selectedAlgorithm;

   try {
      threshold = boost::lexical_cast<int>(thresholdArg);
      if(threshold > 100.0f || threshold < 1.0f)
         throw 33; //No specific reason for 33, just felt like it
   } catch (std::exception& e) { 
      std::cout << "error : threshold must be between 1 and 100\n\n";
      printUsageAndDie();
   }

   std::cout << "Algorithm : " << tmpSelectedAlgorithm;
   std::cout << ", Threshold : " << threshold << " %" << std::endl;

   fileHashGrouper.threshold = 1-threshold/100;

   std::cout << "Scanning target folder : " << targetFolder << std::endl;
   std::vector<FileHash> fileList;
   listImages(bf::path(targetFolder), fileList);
   std::cout << "Done. " << fileList.size() << " file(s) to scan.\nScanning ...\n";
   int scanned = 0;
   for(int i = 0; i < fileList.size(); i++) {
      fileList[i].compute(selectedAlgorithm);
      printProgressBar(std::floor(((i+1)/static_cast<float>(fileList.size()))*100));
   }
   std::cout << std::endl;
    
   ofstream outputHtml;
   if(enableHtmlOutput) {
      outputHtml.open("results.html");
      if(!outputHtml.is_open()) {
         std::cout << "Error : Could not open 'result.html' for writing, disabling HTML output.";
         std::cout << std::endl;
         enableHtmlOutput = false;
      }
      outputHtml << "<!DOCTYPE html>\n";
      outputHtml << "<html>\n\t<head>\n\t\t<title>Image comparaison result</title>\n";
      outputHtml << "\t\t<link href='style.css' rel='stylesheet' type='text/css'>\n\t</head>\n";
      outputHtml << "\t<body>\n";
      outputHtml << "\t\t<h1>Image comparaison result for " << targetFolder << "</h1>\n";
      outputHtml << "\t\t<h2>" << fileList.size() << " image(s)</h2>\n";
      outputHtml << "\t\t<table>\n";
   }
   
   std::vector<FileHashGroup> groups = fileHashGrouper.computeGroups(fileList);
 
   for(int i = 0; i < groups.size(); i++) {
      if(enableHtmlOutput) {
         outputHtml << "\t\t\t<tr>\n";
         outputHtml << "\t\t\t\t<td>\n";
         groups[i][0].file.printHtml(outputHtml, 4);
         outputHtml << "\t\t\t\t</td>\n";
      }
      #ifdef DEBUG
      std::cout << groups[i][0].file.path() << " ";
      #endif
      for(int j = 1; j < groups[i].elements().size(); j++) {
         if(enableHtmlOutput) {
            outputHtml << "\t\t\t\t<td>\n";
            groups[i][j].file.printHtml(outputHtml, 5);
            outputHtml << "\t\t\t\t<br/><span class='similarity'>";
            outputHtml << groups[i][j].similarity.percentage();
            outputHtml << " %</span>\n";
            outputHtml << "\t\t\t\t</td>\n";
         }
         #ifdef DEBUG
         std::cout << groups[i][j].file.path() << " ";  
         std::cout << "dct : " << groups[i][j].similarity.dct << " | ";
         std::cout << "mh : " << groups[i][j].similarity.mh << " | ";
         std::cout << "radish : " << groups[i][j].similarity.radish << " | "; 
         std::cout << std::endl;
         #endif
      }
      if(enableHtmlOutput)
         outputHtml << "\t\t\t</tr>\n";
   }
   if(enableHtmlOutput) {
      outputHtml << "\t\t</table>\n\t</body>\n</html>";
      outputHtml.close();
      std::cout << "Results saved to 'results.html'" << std::endl;
   }
   std::cout << "Done." << std::endl;
   return 0;
}

