#include "Stack.hpp"
#include <algorithm>


Stack::Stack (vector<string> aSymbols) {
	acceptedSymbols = aSymbols;
	sz = 0;
}

Stack::~Stack () { }


void Stack::push (string symbol) {
  if (find(acceptedSymbols.begin(), acceptedSymbols.end(), symbol) != acceptedSymbols.end()) {
		if (getSize() == 0) initialSymbol = symbol;
		content.push_back(symbol);
		sz++;
	}
	else
		cout << "The symbol " << symbol << " is not contained in the stack alphabet" << endl;
}

void Stack::push (vector<string> symbols) {
	for (int i = 0; i < symbols.size(); i++) {
		push(symbols[symbols.size() - 1 - i]);
	}
}

string Stack::pop () {
	if (sz > 0) {
		string last = content.back();
		content.pop_back();
		sz--;
		return last;
	}
	return "";
}

string Stack::getTop () {
	if (!content.size())
		return "";

	return content.back();
}

const unsigned Stack::getSize () {
  return sz;
}

const string Stack::getInitialSymbol () {
	return initialSymbol;
}

const vector<string> Stack::getAcceptedSymbols () {
	return acceptedSymbols;
}

const string Stack::getStackLine() {
	string res;

	for (int i = 0; i < content.size(); i++) {
		res += content.at(content.size() - i - 1);
	}

	return res;
}


const void Stack::show () {
	cout << endl << "TOP" << endl;
 	for (int i = 0; i < getSize(); i++)
		if (i < getSize() - 1)
	  		cout << "    | " << content.at(getSize() - i - 1) << " |" << endl;
		else
	                cout << "    |_" << content.at(getSize() - i - 1) << "_|" << endl;
	cout  << endl;
}


const void Stack::showInline () {
	for (int i = 0; i < getSize(); i++) {
		cout << content.at(getSize() - i - 1);
	}
}
