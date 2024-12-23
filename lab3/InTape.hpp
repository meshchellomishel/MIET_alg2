/***
* @author: Rudolf Cicko
* @email: alu0100824780@ull.edu.es / cickogames@gmail.com
* @subject: Complejidad computacional at Universidad de La Laguna
* @year: 2016
* @description: Input tape class that will contain the characters to determine if the string
*               pertain to a specific language.
***/

#ifndef _INTAPE_H_
#define _INTAPE_H_

#include <string>
#include <vector>
#include "Utils.hpp"
#include <algorithm>

using namespace std;

// Input tape
class InTape {
  unsigned inx_;      // index of the actual position.
  string fileName_;   // name of the file.
  vector<string> chars_; // Characters of the input tape.
public:
  InTape ();
  InTape (string fileName);
  ~InTape ();
  void loadFromFile (string fileName);
  void loadFromKeyboard ();
  void reset ();
  string getInput ();
  string read ();                         // Read the actual element of the input tape so the head will move to the right (inx++)
  string getActualChar () { return chars_[inx_]; };   // Get actual char at where the head is pointing without moving it
  bool hasNext ();
  const void show (); // Show the content of the input tape.
  const void showInline ();  // show content in the trace table.
  bool isEmpty() { return chars_.size() == 0; };
};


#endif
