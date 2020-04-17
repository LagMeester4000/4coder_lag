# Lag 4coder Customizations

Based on https://github.com/ryanfleury/4coder_fleury

Things I changed from 4coder_fleury:

* Removed custom cursor code.

## Features

* basic autocompletion (WIP)
* user type highlighting
* moving cursor between cased words
* jumping through Clang errors after calling `build_search`



## Building

* Paste the `4coder_lag` folder into your `4coder/custom` folder
* Open `x64 Native Tools Command Prompt for VS` and navigate to the `4coder/custom`
* Call `"./4coder_lag/build.bat"`



## Autocompletion (WIP)

The thing I missed the most when switching to this editor is autocompletion and I couldn't find any customization that had it, so I made it myself. 

After typing the name of a variable, press `control+forward_slash`, navigate with `shift+down` and `shift+up`, complete with `shift+enter`, close with `shift+escape`.

Autocompletion only works on explicitly typed variables (no `auto`) above the current line. Currently function arguments don't work. Works best on simple structs (no functions, constructors, destructors, access modifiers). I made it to work with my programming style, though I would like it to work with classes for interacting with APIs.

Todo:

* Improve struct parser to make it work better with classes and other features mentioned before.
* Make it work with function arguments.
* Allow listing all variables in the scope of the current procedure. 
* Ignore scopes that cannot be reached.



## Type highlighting

Type highlighting will work on any type that exists in a loaded file, after `lag_load_types` is called. I wanted to call this automatically whenever a file is opened, but it didn't work because the files are loaded asynchronously and there is no callback for when its done.



## 