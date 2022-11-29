# Know-thy-enemy
[![](https://img.shields.io/github/downloads/typedeck0/know-thy-enemy/total?style=plastic)](../../releases)

Tired of commanders saying "They're *twice* our size!"? Well now you can put a number on it!

Counts the amount and type of player enemies (that your squad hits or is hit by) in an arcdps fight instance (resets when arcdps does).

![image](https://user-images.githubusercontent.com/113395677/189776525-a1103ead-7313-458a-83de-9befa86c714b.png)

![image](https://user-images.githubusercontent.com/113395677/189776559-de7d1981-8bff-4dd7-8f07-3062b602bf29.png)

## Build
targeting [ImGui 1.8](https://github.com/ocornut/imgui/tree/v1.80)

msvc x64:
```
cl /LD /EHsc /O2 know_thy_enemy.cpp imgui.lib
```

## Install:
make sure you have [arcdps](https://www.deltaconnected.com/arcdps/)

move the know_thy_enemy.dll([releases](../../releases)) to the folder where the gw2 exe is or the bin64 folder (its in the same folder as your gw2 exe)

then in-game open the arcdps options window (alt-shift-t)

in the Interface tab under Windows should be an option to enable the extension


## Use:
Right click the window to toggle between a history of fights and the current instance

## Comments, concerns, and/or proclamations

post an [issue](../../issues)

hmu in discord typedeck#7119

or in-game at woodel.6318
