#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <pHash.h>

#ifndef __H_FILEHASH
#define __H_FILEHASH

enum HashAlgorithm {
   DCT,
   MH,
   RADISH,
   ALL
};

class FileComparaisonResult {
   public:
      double dct;
      double mh;
      double radish;
      HashAlgorithm algorithm;
      FileComparaisonResult(); 
      double average() const;
      int percentage() const;
};

class FileHash {
   private:
      std::string _path;
      int computeDigest();
      void computeMh();
      int computeHash();
   public:
      ulong64 hash;
      Digest digest;
      uint8_t* mh;
      int mhSize;
      HashAlgorithm computedAlgorithm;
      FileHash(const std::string &lPath);
      ~FileHash();
      friend bool operator==(const FileHash &left, const FileHash &right);
      friend bool operator!=(const FileHash &left, const FileHash &right);
      void compute(HashAlgorithm algorithm = ALL);
      const std::string& path() const;
      double hammingDistance(const FileHash &secondHash) const;
      double hammingDistanceMh(const FileHash &secondHash) const;
      double crosscorr(const FileHash &secondHash) const;
      FileComparaisonResult compareTo(const FileHash &secondHash) const;
      void printHtml(ofstream& output, int indentation = 0) const;
      bool isHashComparaisonPossible(const FileHash &secondHash, const HashAlgorithm algorithm) const;
};

class ComparedFileHash {
   public:
      FileHash file;
      FileComparaisonResult similarity;
      ComparedFileHash(const FileHash &f);
      ComparedFileHash(const FileHash &f, const FileComparaisonResult &s);
      friend bool operator==(const ComparedFileHash &left, const ComparedFileHash &right);
      friend bool operator!=(const ComparedFileHash &left, const ComparedFileHash &right);
};

class FileHashGroup {
   private:
      std::vector<ComparedFileHash> _elements;
   public:
      const std::vector<ComparedFileHash>& elements() const;
      friend class FileHashGrouper;
      const ComparedFileHash& operator[](int n) { return this->_elements[n]; };
      friend bool operator==(const FileHashGroup &left, const FileHashGroup &right);
      friend bool operator!=(const FileHashGroup &left, const FileHashGroup &right);
};

class FileHashGrouper {
   public:
      float threshold;
      bool thresholdRequiredForAll;
      HashAlgorithm algorithm;
      bool removeDuplicateGroups;
      FileHashGrouper(); 
      std::vector<FileHashGroup> computeGroups(const std::vector<FileHash> &hashes) const;
};     

#endif
