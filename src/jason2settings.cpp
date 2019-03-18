/**
 * NAME
 *    jason2settings - translate a json specification to c++ header file and html form
 * 
 * SYNOPSIS
 *    jason2settings [OPTION]...
 * 
 * DESCRIPTION
 *    
 *    A program to read a (possibly double slash commented) json file from stdin and write (to stdout) a C++ struct based settings header file.
 * 
 *    It can also produce a starter html form page to amend those settings.
 * 
 *    Any comments in the json file optionally translate to comments in the header file and optionally translate to tooltips in the form file.
 * 
 *    Fields with comments that include the tag "<PRIVATE>" will appear in the header file but not as a field in the form file.
 *    Children of fields with comments that include the tag "<PRIVATE>" will appear in the header file but not as a field in the form file.
 * 
 * OPTIONS
 * 
 *    -f filename
 *        write an html form page to filename
 * 
 *    -t
 *        transfer json comments to header file
 * 
 *    -n structname
 *        by default, the settings structure is labelled "SETTINGS" and the object is called "settings". This option changes them to "STRUCTNAME" and "structname" respectively.
 * 
 * EXAMPLES
 *  To produce a header file:
 *    jason2settings < mysettings.json > mysettings.h
 * 
 *  To produce a header file with struct called "prefs" rather than "settings":
 *    jason2settings -n prefs < mysettings.json > mysettings.h
 * 
 *  To produce a header file with comments from a commented json file:
 *    jason2settings -t < mycommentedsettings.json > mycommentedsettings.h
 * 
 *  To produce an html form file:
 *    jason2settings -f mysettings.html < mysettings.json
 *  
 *  To produce a header file with comments and an html form file:
 *    jason2settings < mysettings.json -t -f mysettings.html
 * 
 * BUGS/FEATURES
 *  json comments must be single line, double slash only and must not be naked - ie. occur on a line on their own.
 *  json comments must also be immediately preceded by a space eg: "this" : "that", //comment
 *  html ouput is ragged; visual studio code does a good job of formatting it though!
 *  json arrays are NOT implemented - behaviour is undefined.
 *  json files must be properly formatted with newlines after each field.
 **/
 
#include <iostream>
#include <fstream>
#include <ctype.h>
#include "ArduinoJson-v5.13.4.h"
#include <time.h> 
#include <unistd.h>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/circular_buffer.hpp>
#include <bits/stdc++.h>

using namespace std;

bool makeHtmlFile = false; 
bool makeValuesJsFile = true; //FIXME set false when -v option is implemented
char *htmlFormFilename = nullptr;
string valuesJsFilename = "data/valuesJs.js";  //<script src="myscripts.js"></script> HERE TODO FIXME when -v option is implemented
bool makeSnippetFile = false; 
char *snippetFilename = "src/snippets.txt"; //TODO
string structureName = "settings";
string structureLabel = "SETTINGS";
bool transferComments = false; //weave json comments into header file
bool insertTooltips = true; // weave comments as tooltips into html form fields
string writeFunctionText = ""; //text for a function to write settings to file
string readFunctionText = "";  //text for a function to read settings from a file
string valuesFunctionText = "";  //text for a function to read settings from a file
string initValues = ""; //text for html initialisation 

ofstream htmlOutput;
ofstream valuesJs;
ofstream snippetOutput;

map<string,vector<string>> commentsNew; // holds identifier/comment pairs - multiple comments per id are possible as ids are not unique if in different objects
// map<string,vector<string>> dataTypes;   // holds identifier/dataype pairs - ditto

/**
 * @brief Retrieve the first (front) comment for the given key. Copy it. Delete the front comment. Return the copy.
 * @warn Must only be called once per key per line during parsing. (Effectively treats commentsNew as a FIFO.)
 */
const string popFirstComment(const char* key){
  string retval(commentsNew[key].front()); 
  commentsNew[key].erase(commentsNew[key].begin());
  return retval;
}

/**
 * @brief Retrieve the first (front) datatype for the given key. Copy it. Delete the front comment. Return the copy.
 * @warn Must only be called once per key per line during parsing. (Effectively treats dataTypes as a FIFO.)
 */
// const string popFirstDataType(const char* key){
//   string retval(dataTypes[key].front()); 
//   dataTypes[key].erase(dataTypes[key].begin());
//   return retval;
// }

void makeValuesFunctionText(string valueName, bool isCheckBox, bool needsQuotes){
  // if  isCheckbox == true
  // add line retval += String("document.getElementById('router.SSID').checked = ") + "'" + String(this->router.SSID) + "'" + ";\n";
  // eg: if dottedName is "router.SSID",
  // add line retval += String("values['router.SSID'] = ") + "'" + String(this->router.SSID) + "'" + ";\n";
  
  // clog << "makeValuesFunctionText: valueName = " << valueName << endl;
  if (isCheckBox){
    valuesFunctionText += R"(    retval += String("document.getElementById(')";
    valuesFunctionText += valueName;
    valuesFunctionText += R"(').checked = "))";
    valuesFunctionText += R"( + String(this->)"; 
    valuesFunctionText += valueName;
    valuesFunctionText += ")";
    valuesFunctionText += R"( + ";\n";)";
    valuesFunctionText += "\n";
  }
  else{
    valuesFunctionText += R"(    retval += String("values[')";
    valuesFunctionText += valueName;
    valuesFunctionText += R"('] = "))";
    if (needsQuotes) valuesFunctionText += R"( + "'")";
    valuesFunctionText += R"( + String(this->)";
    valuesFunctionText += valueName;
    valuesFunctionText += ")";
    if (needsQuotes) valuesFunctionText += R"( + "'")";
    valuesFunctionText += R"( + ";\n";)";
    valuesFunctionText += "\n";
  }
  //retval += String("values['weather.publishDataPeriodMs'] = " ) + String(this->weather.publishDataPeriodMs) + ";\n";
  //clog << retval << endl;
}

/**
 * @brief Write struct and html statements for the given JsonObject
 * @warn Recursive function
 */
void iterateObject(JsonObject& jo, std::ostream& stream, const int level = 0,
 const bool parentIsPrivate = false, const bool parentIsReadOnly = false,
 string fullValueName = "", string fullSquaredName="root", string fullDottedName = "this->"){

	char definition[20] = "";
  string asType;
	for (JsonPair &p : jo)
	{
    string theComment = popFirstComment(p.key);
    string tooltipText = theComment;
    boost::replace_all(tooltipText, R"(//)", ""); //remove slashes from comment for tooltip text
    bool elementIsPrivate = ( tooltipText.find("<PRIVATE>") != string::npos );
    bool elementIsReadOnly = ( tooltipText.find("<READONLY>") != string::npos );
		if (elementIsReadOnly) boost::replace_all(tooltipText, R"(<READONLY>)", ""); //remove <READONLY> from comment for tooltip text
    
    if (p.value.is<JsonObject>()){
      // dataTypes[p.key].push = "obj"
      // create nested array
      // readFunctionText += p.key;
      // readFunctionText +=  + ".";

      writeFunctionText += "    "; //fixed 4 space indent :(
      writeFunctionText += fullSquaredName;
      if (level > 0) writeFunctionText += ".as<JsonObject>()";
      writeFunctionText += R"(.createNestedObject(")";
      writeFunctionText += p.key;
      writeFunctionText += R"(");)";
      writeFunctionText += "\n";
      // string myFullName = parentName + "[" + p.key;
      // myFullName += "]";

      if (makeHtmlFile && !elementIsPrivate && !parentIsPrivate){
        if (level == 0){ //at top (root) level; make a table
          htmlOutput << "<table ";
          if  (insertTooltips) htmlOutput << "title='" << tooltipText; 
          htmlOutput << "' name='" << p.key << "' style=\"background-color: rgba(128, 128, 128, 0.5)\"><tr><th colspan=\"2\">" << p.key << ".</th></tr>";
        }
        else{ //already in an object table - make a "sub table"
          htmlOutput << "<tr><td colspan=\"2\"><table ";
          if  (insertTooltips) htmlOutput << "title='" << tooltipText; 
          htmlOutput << "' name='" << p.key << "' style=\"margin-left: 10%; background-color: rgba(128, 128, 128, 0.5)\"><tr><th colspan=\"2\">" << p.key << ".</th></tr>";
        }
      }

			JsonObject& o = p.value.as<JsonObject&>();
			stream << std::string(level+2,' ') << "struct " ;
      
      //add upper case struct label
      for (int i = 0; p.key[i]; i++){
        stream << (char)toupper(p.key[i]);
      }
      stream << " {" << std::endl;

			iterateObject(o, stream, level + 2, elementIsPrivate || parentIsPrivate, elementIsReadOnly || parentIsReadOnly,
                    fullValueName + (level>0? "." : "") + p.key,
                    fullSquaredName + R"([")" + p.key + R"("])",
                    fullDottedName + (level>0? "." : "") + p.key );

      if  (makeHtmlFile){
        //end table or sub table
        if (level == 0){ //at top (root) level; end table
          htmlOutput << "</table>";
        }
        else{ //end sub table
          htmlOutput << "</table></tr></td>";
        }
      }

			stream << std::string(level+2,' ') << "}" << p.key  << ";" << std::endl; 
		}
		else{ //not an object
      char* fieldType;
      bool includeValueInQuotes = false;
			if      (p.value.is<bool>()){   strcpy(definition,"bool");   fieldType = "checkbox"; asType = "as<bool>";}
			else if (p.value.is<long>()){   strcpy(definition,"long");   fieldType = "number"; asType = "as<long>";}
			else if (p.value.is<double>()){ strcpy(definition,"double"); fieldType = "number"; asType = "as<double>";}
			else if (p.value.is<char*>()){  strcpy(definition,"String"); fieldType = "text"; asType = "as<char*>";includeValueInQuotes = true;}
			// else if (p.value.is<JsonArray>()){ strcpy(definition,"char *"); fieldType = "text";} ARRAY NOT IMPLEMENTED
			else {strcpy(definition,"// unknown type"); fieldType = "text";}
      // dataTypes[p.key].push_back(definition);


      readFunctionText += "    "; //fixed 4 space indent :(
      //replace settings.xxx with this->xxx
      //string s(fullDottedName);
      // boost::replace_all(s,"settings.","this->"); //disgusting! TODO
      // boost::replace_all(s,"settings","this->"); //disgusting! TODO
      string dottedName = fullDottedName;
      if (level > 0) dottedName += ".";
      dottedName += p.key;
      dottedName += " = ";
      dottedName += fullSquaredName;
      dottedName += R"([")";
      dottedName += p.key;
      dottedName += R"("].)";
      dottedName += asType;
      dottedName += R"(();)";

      // readFunctionText += fullDottedName;
      // if (level > 0) readFunctionText += ".";
      // readFunctionText += p.key;
      // readFunctionText += " = ";
      // readFunctionText += fullSquaredName;
      // readFunctionText += R"([")";
      // readFunctionText += p.key;
      // readFunctionText += R"("].)";
      // readFunctionText += asType;
      // readFunctionText += R"(();)";

      readFunctionText += dottedName;
      readFunctionText += "\n";

      writeFunctionText += "    "; //fixed 4 space indent :(
      writeFunctionText += fullSquaredName;
      writeFunctionText += R"([")";
      writeFunctionText += p.key;
      writeFunctionText += R"("] = )";
      writeFunctionText += fullDottedName; 
      if (level > 0) writeFunctionText += ".";
      writeFunctionText += p.key;
      writeFunctionText += ";\n"; 

      string valueName = fullValueName;

      if (makeValuesJsFile && !elementIsPrivate && !parentIsPrivate){  //FIXME if required when -v option is implemented
        //add a value eg: value['router.SSID'] = '%router.SSID%'; to secondary script file
        if (level > 0) valueName += ".";
        valueName += p.key;
        makeValuesFunctionText(valueName, p.value.is<bool>(), includeValueInQuotes);
      //below is probably redundant if we use valuesJs.js script TODO
        initValues += R"(values[")";
        initValues += valueName;
        initValues += R"("] = )";
        if (includeValueInQuotes) initValues += R"(")";
        initValues += "%";
        initValues += valueName;
        initValues += "%";
        if (includeValueInQuotes) initValues += R"(")";
        initValues += ";";
        initValues += "\n";
      }

      //snippets - webServer handle submitted form
      if (makeSnippetFile && !elementIsPrivate && !parentIsPrivate && !elementIsReadOnly && !parentIsReadOnly){
        string str = structureName + ".";
        if (p.value.is<bool>()){ 
          snippetOutput << "  " << str << valueName << R"( = webServer.hasArg(")" << valueName << R"(");)" << endl << endl;
        }
        else if (p.value.is<long>()){ 
          snippetOutput << R"(  if (webServer.hasArg(")" << valueName << R"(")))" ;
          snippetOutput << "{\n    "  << str << valueName << " = " << R"(atol(webServer.arg(")" << valueName << R"(").c_str());)" << endl << R"(  })" << endl << endl;
        }
        else if (p.value.is<double>()){ 
          snippetOutput << R"(  if (webServer.hasArg(")" << valueName << R"(")))" ;
          snippetOutput << "{\n    "  << str << valueName << " = " << R"(atof(webServer.arg(")" << valueName << R"(").c_str());)" << endl << R"(  })" << endl << endl;
        }
        else{
          snippetOutput << R"(  if (webServer.hasArg(")" << valueName << R"(")))" ;
          snippetOutput << "{\n    "  << str << valueName << " = " << R"(webServer.arg(")" << valueName << R"(");)" << endl << R"(  })" << endl << endl;
        }
      }

      if (makeHtmlFile && !elementIsPrivate && !parentIsPrivate){
        //add an input field
        if (level > 0){ //inside a table - add a new row
          htmlOutput << "<tr><td><label>" << p.key << "</label></td>";
          htmlOutput << "<td><input type='" << fieldType << "' id='" << valueName << "' name='" << valueName;
          if (!strcmp(fieldType,"checkbox")){
            if (p.value) htmlOutput << "' checked";
            else htmlOutput << "'";
          }
          else {
            htmlOutput << "' value=" << p.value;
          }
          if (insertTooltips) htmlOutput << " title='" << tooltipText <<"'";
          if (elementIsReadOnly || parentIsReadOnly){
            htmlOutput << " disabled";
          }
          htmlOutput << " maxlength='60'></td></tr>";
        }
        else{ //outside a table - make one just for this element
          htmlOutput << "<table><tr><td><label>" << p.key << "</label></td>";
          htmlOutput << "<td><input type='" << fieldType << "' id='" << valueName << "' name='" << valueName;
          if (!strcmp(fieldType,"checkbox")){
            if (p.value) htmlOutput << "' checked";
            else htmlOutput << "'";
          }
          else {
            htmlOutput << "' value=" << p.value;
          }
          if (insertTooltips) htmlOutput << " title='" << tooltipText <<"'";
          if (elementIsReadOnly || parentIsReadOnly){
            htmlOutput << " disabled";
          }
          htmlOutput << " maxlength='60'></td></tr></table>";
        }
      }

			stream << std::string(level+2,' ') << definition << " " << p.key << " = " << p.value << ";";
      if (transferComments) stream << " " << theComment;
      stream << std::endl;
		}
	}
} //iterateObject


int runParser(string uncommentedJson){

// Allocate JsonBuffer
  const int capacity = 20000 ; // = 10 * size from assistant See https://arduinojson.org/v5/assistant/
  StaticJsonBuffer<capacity> jb;
  // strip comments before handing to arduinojson parser
  string line;
  string theInput;
  while(getline(cin,line)){
    size_t pos = line.find("//");
    if ( pos != std::string::npos){
      line = line.substr(0, pos-1);
    }
    theInput += line;
  }

  JsonObject& root = jb.parseObject(uncommentedJson);
  if (!root.success()) {
    cerr << "Parsing failed, check the input json file content. Did you strip the comments out?" << endl; //does root say where the error occurred? TODO
    return -2;
  }
  
  if (makeSnippetFile){
    snippetOutput.open(snippetFilename);
    if (!snippetOutput) {
      cerr << "Failed to open html file " << snippetFilename << " for output. Continuing without snippet output..." << endl;
      makeSnippetFile = false;
    }
    else{
      snippetOutput << R"(//void handleSubmitSettings(){)" << endl;
    }
  }

  if (makeHtmlFile){
    htmlOutput.open(htmlFormFilename);
    if (!htmlOutput) {
      cerr << "Failed to open html file " << htmlFormFilename << " for output. Continuing without html output..." << endl;
      makeHtmlFile = false;
    }
    else{
      if (makeValuesJsFile){
        valuesJs.open(valuesJsFilename);
        if (!valuesJs) {
          cerr << "Failed to open javascript file " << valuesJsFilename << " for output. Continuing nonetheless..." << endl;
          //makeValuesJsFile = false; FIXME when -v option is implemented
        }
      }
    }
  }
  if (makeHtmlFile){
    htmlOutput << R"(
    <!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="settings" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="X-UA-Compatible" content="HTML,CSS">
    <style>
        input:disabled {
          border-style: none;
          background-color: #ffffffff;
        }
        th,
        td {
            padding: 2px;
            width: 50%
        }
    
        body {
            background-color: beige;
        }
    
        button {
            display: block;
            text-align: center;
            width: 90%;
        }
    
        table {
            width: 90%;
            border-spacing: 5px;
            margin-left: auto;
            margin-right: auto;
            border-style: ridge;
            border-width: 5px
        }
    
        label {
            display: block;
            text-align: right
        }
    
        h2 {
            display: block;
            text-align: center
        }
    </style>
    <title>ESP Settings</title>
</head>

<body>
    <form class='w3-container' action='/submitSettings' method='get'>
        <div style="width: 80%;">
            <h2>settings.</h2>
    )";
  }


  time_t rawtime;
  struct tm * timeinfo;
  char buf [80];

  time (&rawtime);
  timeinfo = localtime (&rawtime);
  strftime (buf,80,"%d%b%y %H:%M.",timeinfo);

  writeFunctionText = R"(
  bool write() {
    DynamicJsonBuffer jb(JSON_BUF_SIZE);
    JsonObject &root = jb.createObject();
)";

// readFunctionText = R"(
//   bool read() {
//     DynamicJsonBuffer jb(JSON_BUF_SIZE);
//     File settingsFile = SPIFFS.open(this->filename, "r");
//     if (!settingsFile) return false;
//     JsonObject &root = jb.parseObject(settingsFile);
//     if (!root.success()) return false;
// )";

readFunctionText = R"(
  int read() {
    DynamicJsonBuffer jb(JSON_BUF_SIZE);
    IN(this->filename);
    if (!settingsFile) return READ_FILE_NOT_FOUND;
    JsonObject &root = jb.parseObject(settingsFile);
    if (!root.success()) return READ_PARSE_FAIL;
    if (this->version != root["version"].as<char*>()) return READ_VERSION_NO_MATCH;
)";


  cout << "// Generated on " << buf << endl << endl;
	cout << "#pragma once" << endl << endl;
  cout << R"(
#ifdef Arduino_h
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#define JSON_BUF_SIZE 3000

#define OUT(f)\
    File settingsFile = SPIFFS.open(f, "w");\
    root.prettyPrintTo(settingsFile);\
    settingsFile.close();
    
#define IN(f)\
    File settingsFile = SPIFFS.open(this->filename, "r");

#else
#include <fstream>
#include <string>
#include "ArduinoJson-v5.13.4.h"

#define String string
#define JSON_BUF_SIZE 3000

#define OUT(f)\
    string buf;\
    root.prettyPrintTo(buf);\
    ofstream settingsFile(f);\
    settingsFile << buf;\
    settingsFile.close();
#define IN(f)\
    ifstream settingsFile;\
    settingsFile.open(this->filename);
#endif
)";



  // cout << "#include <Arduino.h>" << endl;
  // cout << "#include <ArduinoJson.h>" << endl;
  // cout << "#include <FS.h>" << endl;
	// cout << "#include <string>" << endl << endl;
  // cout << "#define JSON_BUF_SIZE_ESP8266 3000" << endl; //check size at https://arduinojson.org/v5/assistant/
  cout << R"(
#define READ_OK 0
#define READ_PARSE_FAIL 1
#define READ_VERSION_NO_MATCH 2
#define READ_FILE_NOT_FOUND 3

)";
	cout << "using namespace std;" << endl << endl;
  // cout << "struct SETTINGS {" << endl;
  cout << "struct " << structureLabel << "{" << endl;


  iterateObject(root, cout, 0); //write .h and html form
  // clog << "initValues:" << endl << initValues <<endl << "END initValues" << endl;

	// writeFunctionText += R"(
  //   File settingsFile = SPIFFS.open(this->filename, "w");
  //   root.prettyPrintTo(settingsFile);
  //   return true;
  // )";
	writeFunctionText += R"(
    OUT(this->filename);
    return true;
  )";
  writeFunctionText += R"(}//write)";

	readFunctionText += R"(
    settingsFile.close();
    return READ_OK;
  )";
  readFunctionText += R"(}//read)";
  
  cout << writeFunctionText << endl;
  cout << readFunctionText << endl;

  cout << "  String getValuesScript(){\n";
  cout << R"(    String retval = "";)" << endl;
  
  cout << R"(    retval += String("var values = {};") + "\n";)" << endl; //HERE

  cout << valuesFunctionText << endl;

  cout << R"(    retval += "for (var key in values) {";)" << endl;
  cout << R"(    retval += "  document.getElementById(key).value = values[key];";)"  << endl;
  cout << R"(    retval += "}";)" << endl;

  cout << "    return retval;" << endl;
  cout << "  }//getValuesScript\n" << endl;

  cout << "} " << structureName << ";" << endl;

  if (makeValuesJsFile){
    valuesJs << initValues << endl;
    valuesJs.close();
  }

  if (makeSnippetFile){
    snippetOutput << R"(//}//handleSubmitSettings)";
    snippetOutput.close();
  }

  if (makeHtmlFile){
    htmlOutput << R"(
            <table style="border-style: hidden;">
                <tr>
                    <td><button style="width:100%;" type='submit'>Save Settings</button></td>
                </tr>
            </table>
          </div>
        </form>
        )" << endl;
    
    //insert values script
    // htmlOutput << R"(<script src="valuesJs.js"  defer type="text/javascript"> </script>)" << endl;
    htmlOutput << R"(<script src="valuesJs.js" type="text/javascript"> </script>)" << endl;

    //insert replace script
    // htmlOutput << R"(
    //     <script type="text/javascript">
    //     //var values = {};
    //     )";

    // htmlOutput << R"(
    //     for (var key in values) {
    //         document.getElementById(key).value = values[key];
    //     }
    //     </script>
    // )" << endl;

    htmlOutput << R"( 
      </body>
    </html>
    )" << endl;
    htmlOutput.close();
  }
  return 0;

}

int main(int argc, char *argv[]){
  for (int i = 0; i < argc; i++){
    if ( !strcmp(argv[i], "-f") && (i + 1 < argc) ){
      clog << "Writing html form to " << argv[i + 1] << endl;
      clog << "Writing values.js to " << valuesJsFilename << endl;
      makeHtmlFile = true;
      htmlFormFilename = argv[i + 1];
      continue;
    }
    if ( !strcmp(argv[i], "-s") && (i + 1 < argc) ){
      clog << "Writing snippet code to " << argv[i + 1] << endl;
      makeSnippetFile = true;
      snippetFilename = argv[i + 1];
      continue;
    }
    if ( !strcmp(argv[i], "-t") ){
      clog << "Will transfer comments to header file." << endl;
      transferComments = true;
      continue;
    }
    if ( !strcmp(argv[i], "-n") && (i + 1 < argc) ){
      clog << "Refer to settings structure as " << argv[i + 1] << endl;
      structureName = argv[i + 1];
      structureLabel = boost::to_upper_copy<std::string>(structureName);
      continue;
    }
}

    string line;
    string uncommentedJson;  //will contain an uncommented version of json for ArduinoJson
    while (getline(cin, line))
    {
      string id;
      string comment("");
      vector<string> result;
      boost::split(result, line, boost::is_any_of(":"));
      boost::replace_all(result[0], R"(")", ""); //remove json quotes around identifier
      id = result[0];
      id.erase(std::remove_if(id.begin(), id.end(), [](unsigned char x) { return std::isspace(x); }), id.end()); //remove whitespace

      size_t pos = line.find("//");
      if (pos != std::string::npos)
      {
        comment = line.substr(pos, 255);
        uncommentedJson += line.substr(0, pos-1);
      }
      else{
        uncommentedJson += line;
      }
      commentsNew[id].push_back(comment);
    }
    runParser(uncommentedJson);
}