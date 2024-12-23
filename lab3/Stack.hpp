/***
* @author: Rudolf Cicko
* @email: alu0100824780@ull.edu.es / cickogames@gmail.com
* @subject: Complejidad computacional at Universidad de La Laguna
* @year: 2016
* @description: Stack class used for the Automaton to push and pop characters depending on the transition functions.
*
***/
#ifndef _STACK_HPP
#define _STACK_HPP
#include <iostream>
#include <string>
#include <vector>

using namespace std;

class Stack {
  vector<string> acceptedSymbols;  // Símbolos que acepta la pila.
  vector<string> content; // Contenido de la pila.
  unsigned sz;
  string initialSymbol;

public:
  Stack (vector<string> aSymbols);
  ~Stack ();

  void push (string symbol);
  void push (vector<string> symbols);
  string pop ();
  string getTop ();
  const void show ();        // show content more beautiful.
  const void showInline ();  // show content in the trace table.

  // Getters
  const unsigned getSize();
  const string getInitialSymbol ();
  const vector<string> getAcceptedSymbols ();
  const string getStackLine();
};

#endif
