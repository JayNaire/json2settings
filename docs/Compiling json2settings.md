### To make json2settings :
(Note: You need C++11 as a minimum; probably; I think!)

```
cd downloadfolder
make
```
Observe "downloadfolder/bin/json2settings" has been created.

####To make json2settings available from command line etc. :
add lines to ~/.bashrc  (or wherever):
```
#to find json2settings - for making settings files (ESP8266) from json specs
PATH="$PATH:$HOME/downloadfolder/bin"
```
#####To test basic functioning of json2settings :
From a terminal :
```
json2settings
```
json2settings is waiting for input.
Paste this json (including the empty line) into the terminal:
```
{
  "version" : "1.0", //<READONLY> Field will be disabled in html form
  "secretNumberSetting" : 12345, //<PRIVATE> Field will not appear in html form
  "myFirstSetting" : "It worked",
  "myFavouriteAuthor" : "Jane Eyre",
  "myBooleanSetting" : true //will appear as a checkbox on html form
}

```
Hit ctrl+D to finish.
Observe output header text.