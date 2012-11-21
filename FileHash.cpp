#include "FileHash.h"

namespace bf=boost::filesystem;

#pragma mark FileComparaisonResult

FileComparaisonResult::FileComparaisonResult() {
   this->dct = -1;
   this->mh = -1;
   this->radish = -1;
   this->algorithm = ALL;
}

double FileComparaisonResult::average() const {
   switch(algorithm) {
      case DCT:
         return this->dct;
      case MH:
         return this->mh;
      case RADISH:
         return this->radish;
      case ALL:
         return (this->dct+this->mh+this->radish)/3;
   }
}

int FileComparaisonResult::percentage() const {
   return (1-this->average())*100;
}

#pragma mark FileHash

FileHash::FileHash(const std::string &lPath) {
   this->_path = lPath;
   this->_hash = 0ul;
   this->_mh = NULL;
   this->_mhSize = 0;
   this->_computedAlgorithm = DCT;
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
   std::cout << "\n" << this->_path << "\n";
   #endif
   switch(algorithm) {
      case DCT:
         this->computeHash();
         break;
      case MH:
         this->computeMh();
         break;
      case RADISH:
         this->computeDigest();
         break;
      default:
         this->computeAll();
         break;
   }
   this->_computedAlgorithm = algorithm;
}

void FileHash::computeAll() {
   this->computeHash();
   this->computeDigest();
   this->computeMh();
}

int FileHash::computeDigest() {
   return ph_image_digest(this->_path.c_str(), 1.0f, 1.0f, this->_digest); 
}

void FileHash::computeMh() {
   _mh = ph_mh_imagehash(this->_path.c_str(), this->_mhSize);
}

int FileHash::computeHash() {
   return ph_dct_imagehash(this->_path.c_str(), this->_hash);
}

//All comparaisons are standardized (unlike the lib)
//0 == similar, 1 == different
double FileHash::hammingDistance(const FileHash &secondHash) const {
   if(!this->isHashComparaisonPossible(secondHash, DCT))
      return -1;
   return (double)ph_hamming_distance(this->_hash, secondHash.hash())/64;
}

double FileHash::hammingDistanceMh(const FileHash &secondHash) const {
   if(!this->isHashComparaisonPossible(secondHash, MH))
      return -1;
   return ph_hammingdistance2(this->_mh, this->_mhSize, secondHash.mh(), secondHash.mhSize());
}

double FileHash::crosscorr(const FileHash &secondHash) const {
   if(!this->isHashComparaisonPossible(secondHash, RADISH))
      return -1;
   double pcc;
   ph_crosscorr(this->_digest, secondHash.digest(), pcc);
   return 1-pcc;
}

ulong64 FileHash::hash() const {
   return this->_hash;
}

const Digest& FileHash::digest() const {
   return this->_digest;
}

const std::string& FileHash::path() const {
   return this->_path;
}

uint8_t* FileHash::mh() const {
   return this->_mh;
}

int FileHash::mhSize() const {
   return this->_mhSize;
}

HashAlgorithm FileHash::computedAlgorithm() const {
   return this->_computedAlgorithm;
}

bool FileHash::compareTo(const FileHash &secondHash, FileComparaisonResult& result) const {
   result.dct = this->hammingDistance(secondHash);
   result.mh = this->hammingDistanceMh(secondHash);
   result.radish = this->crosscorr(secondHash);
   result.algorithm = this->_computedAlgorithm;
   return true;
}

void FileHash::printHtml(ofstream& output, int indentation) const {
   std::string outputBase;
   bf::path path(this->_path);
   for(int i = 0; i < indentation; i++)
      outputBase += '\t'; 
   output << outputBase << "\t<img src='" << this->_path << "' alt='";
   output << this->_path << "' /><br/>\n"; 
   output << outputBase << "\t" << bf::basename(path) << bf::extension(path) << "\n"; 
}

bool FileHash::isHashComparaisonPossible(const FileHash &secondHash, const HashAlgorithm algorithm) const {
   //If one of the files does not implement the requested hash, you can't compare them
   if((this->_computedAlgorithm != algorithm && this->_computedAlgorithm != ALL) || 
      (secondHash.computedAlgorithm() != algorithm && secondHash.computedAlgorithm() != ALL) )
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
   return this->_elements;
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
   this->threshold = -1.0f;
   this->thresholdRequiredForAll = true;
   this->algorithm = ALL;
   this->removeDuplicateGroups = true;
}

std::vector<FileHashGroup> FileHashGrouper::computeGroups(const std::vector<FileHash> &hashes) const {
   std::vector<FileHashGroup> groups;
   FileHashGroup tmpGroup;
   bool isGroupDuplicate;
   FileComparaisonResult cmpResult;
   for(int i = 0; i < hashes.size(); i++) {
      for(int j = 0; j < hashes.size(); j++) {
         if(j == i)
            continue;
         hashes[i].compareTo(hashes[j], cmpResult);
         
         //If you have trouble understanding this condition, think backwards
         //File is skipped (no match) if ONE threshold is required (cli arg)
         //and NONE of the result are below the threshold.
         //If one matches, "continue" is not executed.
         //Otherwise, the sum is checked agaisnt the threshold if all algorithms are
         //used.
         if(this->algorithm == ALL && !this->thresholdRequiredForAll) {
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
      isGroupDuplicate = false;
      for(int k = 0; k < groups.size(); k++) {
         if(groups[k] == tmpGroup) {
            isGroupDuplicate = true;
            break;
         }
      }
      if(!isGroupDuplicate)
         groups.push_back(tmpGroup);
      tmpGroup._elements.clear();
   }
   return groups;
}
