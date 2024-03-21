/*Class: CS270
 *Authors: Connor Leslie and Jonathan Erwin
 *Date: 5/5/2021
 */
#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <cstring>
#include <signal.h>
#include <fstream>
using namespace std;


enum comType {comment, dirChange, setVar, list, unsetVar, execute, quit};
enum varType {tInt, tString, tChar, tDouble, tDir};
const unsigned int ASCII0 = 0x30;


bool run = true;
void SIGINT_handler(int signum){
	//block
	sigset_t mask, prevMask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigprocmask(SIG_BLOCK, &mask, &prevMask);


	//unblock
	sigprocmask(SIG_SETMASK, &prevMask, NULL);	
}

struct variable{
	string name;
	uint64_t var;//if string this is empty
	string strVar;
	varType type;
};

variable getVariable(uint64_t givenVar, varType givenType){
	variable returnVariable;
	returnVariable.var = givenVar;
	returnVariable.type = givenType;
	return returnVariable;
}

void getAndParse(vector<string> &returnVector){
	returnVector.clear();
	
	//input string
	string input;
	getline(cin,input);
	
	if (cin.eof())
		run = false;

	string token = "";
	char inputChar;
	for(int i = 0; i < (int)input.size(); i++){
		inputChar = input.at(i);
		if((inputChar != ' ') && (inputChar != '\t')){
			token = token + inputChar;
		}else{
			returnVector.push_back(token);
			token = "";
		} 
	}
	returnVector.push_back(token);	
	
	//remove null tokens from returnVector
	for(int j = 0; j < (int)returnVector.size(); j++){
		if(returnVector.at(j).compare("") == 0){
			returnVector.erase(returnVector.begin() + j);
			j--;
		}
	}
	return;
}

//determine the attempted command type
comType determineComType(vector<string> input){
	char firstLetter = input.at(0).at(0);
	string firstWord = input.at(0);
	if(firstLetter == '#'){
		return comment;
	}
	if(firstLetter == '!'){
		return execute;
	}
	if(firstWord.compare("cd")==0){
		return dirChange;
	}
	if(firstWord.compare("lv")==0){
		return list;
	}
	if(firstWord.compare("unset")==0){
		return unsetVar;
	}
	if(firstWord.compare("quit")==0){
		return quit;
	}
	return setVar;

}




int main(){
	//install signal handler
	struct sigaction SIGINT_action;
	SIGINT_action.sa_handler = &SIGINT_handler;
	sigaction(SIGINT, &SIGINT_action, NULL);

	vector<string> input;
	vector<variable> varList;
	
	
	string PATH = "/bin:/usr/bin";
	char buffer[256];
	string CWD = getcwd(buffer, 256);
	string PS = "ctl&&jwr :: ";
	
	
	while(run){
		cout << PS;
		input.clear();
		getAndParse(input);
		//if only whitespace is inserted don't do anything
		if(!input.empty()){
			comType currComType = determineComType(input);
					
			switch (currComType){
				case comment:
					break;
				case dirChange:
				{//enclosing for dirChange
					if(input.size() != 2){
						cout << "dirChange ERROR: Invalid number of parameters.\n";
						break;
					}
					
					if(input.at(1).compare("../")==0){
						int indexOfLastDash = -1;
						int y = 0;
						for(; y < (int)CWD.size(); y++){
							if(CWD.at(y) == '/'){
								indexOfLastDash = y;
							}
						}
						if(indexOfLastDash == -1){
							cout<< "dirChange ERROR: no parent directory.\n";
							break;
						}
						
						CWD = CWD.substr(0, CWD.size()-(CWD.size()-indexOfLastDash));//changed

						break;
					}
					if(input.at(1).compare("./")==0){
						break;//do nothing
					}
					
					
					string token2 = input.at(1);
					vector<string> tokensByDash;
					bool flag = true;
					while(flag){
						int posOfDash = -1;
						int r = 2;
						for(; r < (int)token2.size(); r++){	
							if(token2.at(r) == '/'){
								posOfDash = r;
								break;
							}
						}
						if(posOfDash == -1){
							flag = false;
						}else{
							// removes '/'
							tokensByDash.push_back(token2.substr(0, r-1));
							token2 = token2.substr(r+1, token2.size()-r-1);
						}
					}
					tokensByDash.push_back(token2);
					
					for(int j = 0; j < (int)tokensByDash.size(); j++){
						for(int k = 0; k < (int) varList.size(); k++){
							if(tokensByDash.at(j).at(0) == '$'){
								string actualName = tokensByDash.at(j).substr(1,tokensByDash.at(j).size()-1);
								if(varList.at(k).type != tDir){
									break;
								}else if(actualName.compare(varList.at(k).name)== 0){
									//erase and add
									tokensByDash.erase(tokensByDash.begin()+j);
									tokensByDash.insert(tokensByDash.begin()+j, varList.at(k).strVar);
								}
							}
						} 
					}
					
					string inputDir = tokensByDash.at(0);
					for(int i = 1; i < (int)tokensByDash.size(); i++){
						inputDir = inputDir + "/" + tokensByDash.at(i);
					}
					//check for is the inputDir + CWD a real directory	
					struct stat s;
					string possCWD = CWD+ inputDir;
					if(lstat(possCWD.c_str(), &s) == 0){
						if(S_ISDIR(s.st_mode)){
							CWD = possCWD;
							//was a good directory
						}else{
							cout << "dirChange() ERROR: input was not a valid directory\n";
						        cout << "\tdirectory changes must start with / or with a directory Variable\n";	
						}
					}else{
						//error
						cout << "lstat() return non zero value\n";
						cout << "dirChange() ERROR: input was not a valid directory\n";
					        cout << "\tdirectory changes must start with / or with a directory Variable\n";	
	
					}
					
				}//end ofenclosing of dirChange
					break;
				case setVar://CAN'T CHANGE PATH
				{//enclosing for new Variables
					//tell the choosen comType
					variable newVar;
					
					//find if syntax is correct
					if(input.size() < 3){
						cout << "Undetermined Function Error: if Attempt to setVariable please advise that proper notation is \n\t VARNAME1 = VARNAME2\n";
						break;
					}
					if(input.at(1).compare("=") != 0){
						cout << "setVar Error: no \'=\' token for setVar()\n";
						break;
					}
					if(input.at(0).compare("CWD")==0){
						cout << "CWD cannot be changed using setVar(). Please use cd command\n";
						break;
					}
					if(input.at(0).compare("PATH")==0){
						cout << "PATH cannot be changed\n";
						break;
					}
					//----------------------------------------------------------------------------
					//Case of setVar		PS 
					//----------------------------------------------------------------------------
					if(input.at(0).compare("PS") == 0){
						cout << "Prompt String Change\n";
						//check is if var is string
						if(input.at(2).at(0) == '\"'){
							if(input.back().back() == '\"'){
							
								string varString = "";
							
								//create string for newVar
								for(int i = 2; i < (int)input.size(); i++){
									varString += input.at(i);
									varString += " ";
								}
							
								//setPS
								PS = varString.substr(1,varString.size()-3);
							       	//get rid of ending '\"' and send pointer to location
							}else{
								//throw error if first char in 3rd token starts with \" but last token does not end with \"
								cout << "setVar Error: no ending \" on set string\n";
							}
							
						}else{
							cout << "setVar Error: PS change is set to invalid type.\nPS is of type string. Variable syntax is: VAR = \"NEWPS\".\n";
						}
						break;
					}


					//if variable name is in use add 1 to the variable name
					//will result in cases of variableName111
					bool openNameFound = false;
					while(openNameFound==false){
						openNameFound = true;
						for(int currVar = 0; currVar < (int)varList.size(); currVar++){
							if(input.at(0).compare(varList.at(currVar).name) == 0){
								openNameFound = false;
								cout << "setVar Warning: given name is in use: \n\t In use: " << varList.at(currVar).name << "\n\t Input saved as : " << input.at(0)+ "1" << endl;
								cout << "If you want to overwrite variable: " << varList.at(currVar).name <<
								       	", please first use Command unset() in the format:\n\t unset("<<varList.at(currVar).name << ")\n";
								input.at(0) = input.at(0) + "1";
							}
						}
					}
					newVar.name = input.at(0);

					//--------------------------------------------------------------------------
					//Case of setVar 		tDir
					//--------------------------------------------------------------------------
					
					//hasDash
					bool hasDash = false;
					for(int i = 0; i < (int)input.at(2).size(); i++){
						if(input.at(2).at(i) == '/'){
							hasDash = true;
						}
					}


					if(hasDash){
							
						string token2 = input.at(2);
						vector<string> tokensByDash;
						bool flag = true;
						while(flag){
							int posOfDash = -1;
							int n = 2;
							for(; n < (int)token2.size(); n++){	
								if(token2.at(n) == '/'){
									posOfDash = n;
									break;
								}
							}
							if(posOfDash == -1){
								flag = false;
							}else{
								// removes '/'
								tokensByDash.push_back(token2.substr(0, n-1));
								token2 = token2.substr(n+1, token2.size()-n-1);
							}
						}
						tokensByDash.push_back(token2);
					
						for(int j = 0; j < (int)tokensByDash.size(); j++){
							for(int k = 0; k < (int) varList.size(); k++){
								if(tokensByDash.at(j).at(0) == '$'){
									string actualName = tokensByDash.at(j).substr(1,tokensByDash.at(j).size()-1);
									if(varList.at(k).type != tDir){
										break;
									}else if(actualName.compare(varList.at(k).name)== 0){
										tokensByDash.erase(tokensByDash.begin()+j);
										tokensByDash.insert(tokensByDash.begin()+j, varList.at(k).strVar);
									}
								}
							} 
						}
					
						string inputDir = "";
						for(int i = 0; i < (int)tokensByDash.size(); i++){
							inputDir = inputDir + "/" + tokensByDash.at(i);
						}
					
						
						newVar.strVar = inputDir;
						newVar.type = tDir;
					}

					//-----------------------------------------------------------------
					//Case for setVar:		STRING
					//-----------------------------------------------------------------
					//check is if var is string
					if(input.at(2).at(0) == '\"'){
						cout << "is string"<<endl;
						if(input.back().back() == '\"'){
							
							string varString = "";
							
							//create string for newVar
							for(int i = 2; i < (int)input.size(); i++){
								varString += input.at(i);
								varString += " ";
							}
							if(varString.size() < 3){
								cout << "setVar ERROR: \" is not an acceptable Value\n";
								cout << "\tFor \" character please put '\"'\n";
								break;
							}
							//create newVar
							newVar.type = tString;
							newVar.strVar = varString.substr(1,varString.size()-3);//might need to be 3 not 2
							varList.push_back(newVar);
							break;
						}else{
							//throw error if first char in 3rd token starts with \" but last token does not end with \"
							cout << "setVar Error: no ending \" on set string\n";
						}
						break;
					}
					//------------------------------------------------------------------
					
					//-----------------------------------------------------------------
					//Case for setVar: 		CHAR ' '
					//----------------------------------------------------------------
					//NO HANDLING CASE '\t'-would be saved as a space
					if((input.at(2).compare("\'") == 0) && (input.at(3).compare("\'")) && (input.size() == 4)){
						cout << "is SPACE" << endl;
						char varChar = ' ';
						newVar.type = tChar;
						newVar.var = (uint64_t)varChar;
						varList.push_back(newVar);
						break;
					}
					//----------------------------------------------------------------
					
					//for not string necessary syntax Check
					if(input.size() != 3){
						cout << "setVar Error: invalid number of parameters\n";
						break;
					}
					//not the exact same variable requirements as Required
					//also command overloads ex: 'lv', 'cd', 'quit', and 'unset'
					//	so if these are set as a variable it should not
					//	through any issues


					//----------------------------------------------------------------------
					//Case for setVar:		CHAR
					//----------------------------------------------------------------------
					if((input.at(2).size()==3)  &&  (input.at(2).at(0) == '\'')&&(input.at(2).at(2)=='\'')){
						cout << "isChar"<<endl;
						char varChar = input.at(2).at(1);
						newVar.type = tChar;
						newVar.var = (uint64_t) varChar;
						varList.push_back(newVar);
						break;
					}

					//--------------------------------------------------------------------
					//Case for setVar:		Given Variable
					//-------------------------------------------------------------------
					if(input.at(2).at(0) == '$'){
						cout << "givenVariable"<<endl;
						string givenVarName = input.at(2).substr(1,input.at(2).size()-1);
						
						bool isInList = false;
						for(int i = 0; i < (int)varList.size(); i++){
							if(givenVarName.compare(varList.at(i).name)==0){
								isInList = true;
								newVar.type = varList.at(i).type;
								//if the variable is a String, the variables will
								//point to the same string
								newVar.strVar = varList.at(i).strVar;
								newVar.var = varList.at(i).var;
								varList.push_back(newVar);
								break;
							}
						}
						if(isInList == false){
							//show Error
							cout << "setVar Error: Given token " << givenVarName << "is not in list of variables\n";
						}
						break;
					}
					
					//------------------------------------------------------------
					//Case for setVar:		INT or DOUBLE
					//------------------------------------------------------------
					//does not Handle hex notation: 0x, or bit notation: 0b, or integer
					//	types that don't fit into a long ei: 8 bytes
					bool isCharInt = false;
				        bool isInt = true;
					bool isCharDouble = false;
					bool isDouble = true;
					int numOfDecimals = 0;
					for(long unsigned int i = 0; i < input.at(2).size(); i++){
						isCharInt = false;
						isCharDouble = false;
						for(unsigned int digit = 0; digit < 10; digit++){
							if(input.at(2).at(i) == ((char)(digit+ASCII0))){
								isCharInt = true;
								isCharDouble = true;
							}
						}if(input.at(2).at(i) == '.'){
							isCharDouble = true;
							numOfDecimals++;
						}
							
						

						if(!isCharInt)
							isInt = false;
						if(!isCharDouble)
							isDouble = false;
					}
					
					//is a integer value
					if(isInt){
						cout << "isInt"<<endl;
						long varInt = stol(input.at(2));
						newVar.type = tInt;
						newVar.var = (uint64_t) varInt;
						varList.push_back(newVar);
						break;
					}
					//is a floating Point Value
					if((numOfDecimals == 1) &&(isDouble)){
						cout << "isDouble"<<endl;
						double varDouble = stod(input.at(2));
						
						newVar.type = tDouble;
						memcpy(&newVar.var, &varDouble, 8);
						varList.push_back(newVar);
						break;
					}
					if((numOfDecimals > 1) && (isDouble)){
						cout << "setVar Error: too many . in double assignment\n";
						break;
					}
					
					
				}
				//enclosing for new variabls
					break;
				
				case list:
					{
						cout << "PATH = " << PATH << endl;
						cout << "PS = " << PS << endl;
						cout << "CWD = " << CWD << endl;
						for(int i = 0; i < (int)varList.size(); i++){
							cout << varList.at(i).name << " = ";
							varType currVarType = varList.at(i).type;
							switch (currVarType){
								case tInt:
									cout << (long)varList.at(i).var << endl;
									break;
								case tString:
								{
									if(varList.at(i).strVar.size() < 1){
										cout << "String Print ERROR: Size is < 1.\n";
									}
									cout << "\"";
									cout << varList.at(i).strVar;
									cout << "\"\n";
								}
									break;
								case tChar:
									cout << (char)varList.at(i).var << endl;
									break;
								case tDouble:
								{
									double dummy = 0.0;
									memcpy(&dummy, &varList.at(i).var, 8);
									cout << dummy << endl;
									break;
								}
								case tDir:
									if(varList.at(i).strVar.size() < 1){
										cout << "tDir Print ERROR: Size is < 1.\n";
									}
									cout << varList.at(i).strVar;
									cout << "\n";
									break;

									break;
							}
						}
					}
					break;

				case unsetVar:
					if(input.size()>2){
						cout << "unsetVar ERROR: Too many parameters for function unsetVar(). \n";
						cout << "\tFormat: unset VARIABLENAME\n";
						break;
					}
					if(input.at(1).compare("PATH") == 0){
						cout << "unsetVar ERROR: PATH can not be unset\n";
						break;
					}
					if(input.at(1).compare("PS") == 0){
						cout << "unsetVar ERROR: PS can not be unset\n";
						break;
					}
					if(input.at(1).compare("CWD") == 0){
						cout << "unsetVar ERROR: CWD can not be unset.\nPlease use cd command\n";
						break;
					}
					for(int i = 0; i < (int)varList.size(); i++){
						if(varList.at(i).name.compare(input.at(1))==0){
							varList.erase(varList.begin()+i);
							break;
						}
					}
					break;
				case execute:
					cout << "execute\n";
					{
					//need to fork
					int status;
					pid_t pid = fork();
					if(pid==0){//child so killing this will be exit(0) not break;

						
						if(input.size() < 2){
							cout << "execute ERROR: in correct number of parameters.\n";
							cout << "Reminder to seperate '!' token and the executable by a space.\n";
							exit(0);
						}
						if(input.at(1).size() < 2){
							cout << "execute ERROR: first Parameter is too small\n";
							exit(0);
						}
						if((input.at(1).substr(0,2) != "./") && (input.at(1).at(0) != '/')){//look through PATH for reasonable executtions
							if(input.size() != 2){
								cout << "execute ERROR: no function calls from /bin or /usr/bin are allowed with parameters.\n";
								exit(0);
							}
							
							string possFunc = CWD + "/" + input.at(1);
							struct stat s;
							//break up PATH
							vector<string> pathTokens;
							int lastBreakIndex = 0;
							for(int i = 0; i < (int)PATH.size(); i++){
								if(PATH.at(i) == ':'){
									pathTokens.push_back(PATH.substr(lastBreakIndex, i-lastBreakIndex));
									lastBreakIndex = i+1;
								}
							}
							pathTokens.push_back(PATH.substr(lastBreakIndex, PATH.size()-lastBreakIndex));
							//for testign
							for(int i = 0; i <(int)pathTokens.size(); i++){
								cout << pathTokens.at(i) << endl;
							}
							
							for(int i = 0; i < (int)pathTokens.size(); i++){
								string path = pathTokens.at(i) + "/" +input.at(1);
								if(lstat(path.c_str(), &s) == 0){
									if(S_ISREG(s.st_mode)){
										cout << "is valid Call" << endl;
										char callPath[path.size()+1];
										strcpy(callPath, path.c_str());
										
										char callInput1[input.at(1).size()+1];
										strcpy(callInput1, input.at(1).c_str());
										char* argv1[] = {callInput1, NULL};
										char* envp1[] = {NULL};
										execve(callPath, argv1, envp1);
									}
								}else
									cout << "BIG FAIL\n";
							}
							cout << "FAIL\n";

						}else if(input.at(1).substr(0,2).compare("./") == 0){//execute from CWD
							
							string pathStr = CWD + input.at(1).substr(1,input.at(1).size()-1);
							vector<string> arguments;
							int outToIndex = -1;
							int inFromIndex = -1;
							for(int i = 2; i < (int)input.size(); i++)
							{
								if(input.at(i-1).compare("inFrom:")==0){
									inFromIndex = i;
									arguments.pop_back();
									break;
								}
								else if (input.at(i-1).compare("outTo:")==0){
									outToIndex = i;
									arguments.pop_back();
									break;
								}
								if((outToIndex == -1) && (inFromIndex == -1))
									arguments.push_back(input.at(i));
								
							}
							
							
							char path[pathStr.size()+1];
							strcpy(path, pathStr.c_str());
							
							char* argv[arguments.size()+2];
							argv[0] = path;
							argv[arguments.size()+1] = NULL;
							
							for(int i = 0; i < (int)(arguments.size()); i++){
								argv[i+1] = (char*)arguments.at(i).c_str();
							}	
							
							char* envp[] = {NULL};
							
					
							struct stat s;
							if(lstat(pathStr.c_str(), &s) == 0){
								if(S_ISREG(s.st_mode)){

									if(outToIndex != -1){
										freopen(input.at(outToIndex).c_str(), "w", stdout);
									}
									if(inFromIndex != -1){
										freopen(input.at(inFromIndex).c_str(), "r", stdin);	
									}
									execve(path,argv,envp);
								}else{
									cout << "not a file\n";
								}
						
							}else{
								cout << "execute ERROR: "<< pathStr << "is not a valid path.\n";
								exit(0);
							}
						}else if(input.at(1).at(0) == '/'){//execute from root
						
							string pathStr = input.at(1);
	
							vector<string> arguments;
							int outToIndex = -1;
							int inFromIndex = -1;
							for(int i = 2; i < (int)input.size(); i++)
							{
								if(input.at(i-1).compare("inFrom:")==0){
									inFromIndex = i;
									arguments.pop_back();
									break;
								}
								else if (input.at(i-1).compare("outTo:")==0){
									outToIndex = i;
									arguments.pop_back();
									break;
								}
								if((outToIndex == -1) && (inFromIndex == -1))
									arguments.push_back(input.at(i));
								
							}
							
							
							char path[pathStr.size()+1];
							strcpy(path, pathStr.c_str());
							
							char* argv[arguments.size()+2];
							argv[0] = path;
							argv[arguments.size()+1] = NULL;
							
							for(int i = 0; i < (int)(arguments.size()); i++){
								argv[i+1] = (char*) arguments.at(i).c_str();
							}	
							
							char* envp[] = {NULL};
							
					
							struct stat s;
							if(lstat(pathStr.c_str(), &s) == 0){
								if(S_ISREG(s.st_mode)){

									if(outToIndex != -1){
										freopen(input.at(outToIndex).c_str(), "w", stdout);
									}
									if(inFromIndex != -1){
										freopen(input.at(inFromIndex).c_str(), "r", stdin);	
									}
									execve(path,argv,envp);
								}else{
									cout << "not a file\n";
								}
						
							}else{
								cout << "execute ERROR: "<< pathStr << "is not a valid path.\n";
								exit(0);
							}
						}
					
					}else{//parent
						
						waitpid(pid, &status, 0);
						
					}

					}
					break;
				case quit:
					run = false;
					break;
				default:
					cout << "default\n";
			
			}//end of switch

		}//end of empty Check
		
	}//end of Loop
	return 0;
}//end of main

