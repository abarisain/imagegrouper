#include "FileHash.h"

namespace bf=boost::filesystem;

#pragma mark FileComparaisonResult

FileComparaisonResult::FileComparaisonResult() {
   dct = -1;
   mh = -1;
   radish = -1;
   algorithm = ALL;
}

double FileComparaisonResult::average() const {
   switch(algorithm) {
      case DCT:
         return dct;
      case MH:
         return mh;
      case RADISH:
         return radish;
      case ALL:
         return (dct+mh+radish)/3;
   }
}

int FileComparaisonResult::percentage() const {
   return (1-average())*100;
}

#pragma mark FileHash

FileHash::FileHash(const std::string &lPath) {
   _path = lPath;
   hash = 0ul;
   mh = NULL;
   mhSize = 0;
   computedAlgorithm = DCT;
}

FileHash::~FileHash() {
}

bool operator==(const FileHash &left, const FileHash &right) {
   return boost::iequals(left._path, right._path);
}

bool operator!=(const FileHash &left, const FileHash &right) {
   return !(left == right);
}

void FileHash::compute(HashAlgorithm algorithm) {
   #ifdef DEBUG
   std::cout << "\n" << _path << "\n";
   #endif
   switch(algorithm) {
      case DCT:
         computeHash();
         break;
      case MH:
         computeMh();
         break;
      case RADISH:
         computeDigest();
         break;
      default:
         computeHash();
         computeDigest();
         computeMh();
         break;
   }
   computedAlgorithm = algorithm;
}

int FileHash::computeDigest() {
   return ph_image_digest(_path.c_str(), 1.0f, 1.0f, digest); 
}

void FileHash::computeMh() {
   mh = ph_mh_imagehash(_path.c_str(), mhSize);
}

int FileHash::computeHash() {
   return ph_dct_imagehash(_path.c_str(), hash);
}

//All comparaisons are standardized (unlike the lib)
//0 == similar, 1 == different
double FileHash::hammingDistance(const FileHash &secondHash) const {
   if(!isHashComparaisonPossible(secondHash, DCT))
      return -1;
   return static_cast<double>(ph_hamming_distance(hash, secondHash.hash))/64;
}

double FileHash::hammingDistanceMh(const FileHash &secondHash) const {
   if(!isHashComparaisonPossible(secondHash, MH))
      return -1;
   return ph_hammingdistance2(mh, mhSize, secondHash.mh, secondHash.mhSize);
}

double FileHash::crosscorr(const FileHash &secondHash) const {
   if(!isHashComparaisonPossible(secondHash, RADISH))
      return -1;
   double pcc;
   ph_crosscorr(digest, secondHash.digest, pcc);
   return 1-pcc;
}

const std::string& FileHash::path() const {
   return _path;
}

FileComparaisonResult FileHash::compareTo(const FileHash &secondHash) const {
   FileComparaisonResult result;
   result.dct = hammingDistance(secondHash);
   result.mh = hammingDistanceMh(secondHash);
   result.radish = crosscorr(secondHash);
   result.algorithm = computedAlgorithm;
   return result;
}

void FileHash::printHtml(ofstream& output, int indentation) const {
   std::string outputBase;
   bf::path path(_path);
   for(int i = 0; i < indentation; i++)
      outputBase += '\t'; 
   output << outputBase << "\t<img src='" << _path << "' alt='";
   output << _path << "' /><br/>\n"; 
   output << outputBase << "\t" << bf::basename(path) << bf::extension(path) << "\n"; 
}

bool FileHash::isHashComparaisonPossible(const FileHash &secondHash, const HashAlgorithm algorithm) const {
   //If one of the files does not implement the requested hash, you can't compare them
   if((computedAlgorithm != algorithm && computedAlgorithm != ALL) ||
      (secondHash.computedAlgorithm != algorithm && secondHash.computedAlgorithm != ALL) )
      return false;
   return true;
}

#pragma mark ComparedFileHash

ComparedFileHash::ComparedFileHash(const FileHash &f) : file(f) {

}

ComparedFileHash::ComparedFileHash(const FileHash &f, const FileComparaisonResult &s) : file(f) {
   similarity = s;
}

bool operator==(const ComparedFileHash &left, const ComparedFileHash &right) {
   return (left.file == right.file);
}

bool operator!=(const ComparedFileHash &left, const ComparedFileHash &right) {
   return (left.file != right.file);
}

#pragma mark FileHashGroup

const std::vector<ComparedFileHash>& FileHashGroup::elements() const {
   return _elements;
}

bool operator==(const FileHashGroup &left, const FileHashGroup &right) {
   if(left._elements.size() != right._elements.size())
      return false; 
   bool matchFound;
   for(int i = 0; i < left._elements.size(); i++) {
      matchFound = false;
      for(int j = 0; j < right._elements.size(); j++) {
         if(left._elements[i] == right._elements[j]) {
            matchFound = true;
            break;
         }
      }
      if(!matchFound)
         return false;
   }
   return true;
}

bool operator!=(const FileHashGroup &left, const FileHashGroup &right) {
   return !(left == right);
}

#pragma mark FileHashGrouper

FileHashGrouper::FileHashGrouper() {
   threshold = -1.0f;
   thresholdRequiredForAll = true;
   algorithm = ALL;
   removeDuplicateGroups = true;
}

std::vector<FileHashGroup> FileHashGrouper::computeGroups(const std::vector<FileHash> &hashes) const {
   std::vector<FileHashGroup> groups;
   FileHashGroup tmpGroup;
   bool isGroupDuplicate = false;
   FileComparaisonResult cmpResult;
   for(int i = 0; i < hashes.size(); i++) {
      for(int j = 0; j < hashes.size(); j++) {
         if(j == i)
            continue;
         cmpResult = hashes[i].compareTo(hashes[j]);
         
         //If you have trouble understanding this condition, think backwards
         //File is skipped (no match) if ONE threshold is required (cli arg)
         //and NONE of the result are below the threshold.
         //If one matches, "continue" is not executed.
         //Otherwise, the sum is checked agaisnt the threshold if all algorithms are
         //used.
         if(algorithm == ALL && !thresholdRequiredForAll) {
            if(cmpResult.dct > threshold &&
               cmpResult.mh > threshold &&
               cmpResult.radish > threshold) {
               continue;
            }
         } else if(cmpResult.average() > threshold) {
            continue;
         }
         if(tmpGroup._elements.size() == 0) {
            //First file (reference file) has a dummy similarity
            tmpGroup._elements.push_back(ComparedFileHash(hashes[i]));
         }
         tmpGroup._elements.push_back(ComparedFileHash(hashes[j], cmpResult));
      }
      if(tmpGroup._elements.size() == 0)
         continue;
      if(removeDuplicateGroups) {
         isGroupDuplicate = false;
         for(int k = 0; k < groups.size(); k++) {
            if(groups[k] == tmpGroup) {
               isGroupDuplicate = true;
               break;
            }
         }
      }
      if(!isGroupDuplicate || !removeDuplicateGroups)
         groups.push_back(tmpGroup);
      tmpGroup._elements.clear();
   }
   return groups;
}
