#include "hash.hpp"

using namespace std;

void FindAndReplaceAll(string &data, string toSearch, string replaceStr) {
    // copied from: https://thispointer.com/find-and-replace-all-occurrences-of-a-sub-string-in-c/
    // Get the first occurrence
    size_t pos = data.find(toSearch);
    // Repeat till end is reached
    while (pos != std::string::npos) {
        // Replace this occurrence of Sub String
        data.replace(pos, toSearch.size(), replaceStr);
        // Get the next occurrence from the current position
        pos = data.find(toSearch, pos + replaceStr.size());
    }
}

void PreprocessRawLineStrings(string &l) {
    // must convert the '\n' into \xa here
    FindAndReplaceAll(l, string("\\n"), string("\xa"));
    // must convert the "\"" sequence into a single character '"' here
    FindAndReplaceAll(l, string("\\\""), string("\""));
    // 0x7f symbol for degrees is a similar case
    FindAndReplaceAll(l, string("\\177"), string("\177"));
}
