#Using json2settings

##Warning!

json2settings is a very crudely written CLI app used to gain experience with [ArduinoJson](https://github.com/bblanchon/ArduinoJson/). It has had no testing under Windows and not much under Linux either.

##Introduction
json2settings is intended to aid writing and maintaining settings in an Arduino/ESP8266 application. 
It takes json specs from stdin and :
1.  writes header (.h) text to stdout.
    - In the header text is 
      - a structure (called "settings" by default) with the required fields
      - optional comments (-t option)
      - a function "settings.getValuesScript()" for use by a web server
      - a function "settings.read() that reads in the json settings file specified by settings.filename
      - a function "settings.write() that writes out the json settings file specified by settings.filename
2.  optionally produces an html form based file for maintaining the settings.
3.  optionally produces a .h file for handling web updates in conjunction with the above form file.
###Usage - Linux.
- Write some json to define the variables you want as settings (or any other purpose).
  -  Do not use json arrays; they're not implemented.
  - If you are going to use the html form generated (-f option), avoid using floats and doubles if possible or mark them \<PRIVATE>; they will cause the Arduino rounding grief with String conversion if used in web server html forms later)

Try it out; paste this code <i>(including the last empty line)</i> into a terminal:
   
  ```
  json2settings <<EOF
  {
    "version" : "1.0",            //<READONLY> This field will be disabled in html form
    "filename" : "/settings.json", //<PRIVATE> SPIFFS filename - where to find this very file!
    "secretNumberSetting" : 12345, //<PRIVATE> This private field will not appear in the html form but will appear as normal in settings structure
    "myFirstSetting" : "It worked", //comments appear as tooltips in generated form. Use &#10; for line breaks
    "myFavouriteAuthor" : "Jane Eyre",
    "myBooleanSetting" : true, //will appear as a checkbox on html form
    "diagnostics" :{
      "diagFloat" : 123.45, //any float with more than 5 sig figs will cause trouble!
      "diag2" : 456
    }
  }
  EOF

  ```
Note you can put the json code into a file, "settings.json" say, and achieve the same effect with:
```
json2settings < settings.json
```

Notice there are no comments in the resultant header. <b>To transfer comments</b> use the -t option eg:
```
json2settings -t < settings.json
```
<b>To create a .h file</b> just redirect stdout :
```
json2settings -t < settings.json > mysettings.h
```
To use the header file just include it in your Arduino/ESP code and refer to fields with settings.fieldname.

<b>If you don't like "settings" as the name of the structure</b>, use the -n option:
```
json2settings -t -n preferences < settings.json > mypreferences.h
```

<b>If you want to create an html form page</b> for a webserver, use  the -f option:
```
json2settings -t -n preferences -f preferences.html < settings.json > mysettings.h
```
<b>If you want to create a "snippet" header file </b> for handling web updates, use  the -s option:
```
json2settings -t -n preferences -f preferences.html -s webUpdates.h < settings.json > mysettings.h
```
A complete platformio/ESP8266 application is given in the [examples folder](examples)
##Credits
json2settings grew like topsy after reading Benoit Blanchon's [excellent book](https://arduinojson.org/book/?utm_source=github&utm_medium=readme) on [ArduinoJson](https://github.com/bblanchon/ArduinoJson/).

