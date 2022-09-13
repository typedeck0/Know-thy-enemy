# Know-thy-enemy
Counts the amount and type of player enemies in an arcdps fight instance.

![image](https://user-images.githubusercontent.com/113395677/189776525-a1103ead-7313-458a-83de-9befa86c714b.png)

![image](https://user-images.githubusercontent.com/113395677/189776559-de7d1981-8bff-4dd7-8f07-3062b602bf29.png)

## Build
targeting ImGui 1.8

msvc x64:
```
cl /LD /EHsc /O2 know_thy_enemy.cpp imgui.lib
```

## Install:
make sure you have [arcdps](https://www.deltaconnected.com/arcdps/)

move the know_thy_enemy.dll([releases](../releases)) to the bin64 folder (its in the same folder as your gw2 exe)

then in-game open the arcdps options window (alt-shift-t)

in the Interface tab under Windows should be an option to enable the extension


## Use:
Right click the window to toggle between a history of fights and the current instance

## Comments concerns and/or proclamations

hmu in discord typedeck#7119

or in-game at woodel.6318
