 [Know thy enemy discord](https://discord.gg/6xDbA64rZc) for update notifications and chat.

> Know thy enemy and know thyself, and need not fear the result of a hundred battles.
> 
> \- Sun Tzu

Some parts of the code are closed source, since it is related to arcdps/GW2 internals.

# Know-thy-enemy [WvW]
[![](https://img.shields.io/github/downloads/typedeck0/know-thy-enemy/total?style=plastic)](../../releases)

Tired of commanders saying "They're *twice* our size!"? Well now you can put a number on it!

Counts the amount and type of player enemies (that your squad hits or is hit by) in an arcdps fight instance (resets when arcdps does).

![image](https://user-images.githubusercontent.com/113395677/222940654-ff55d512-85e5-42dc-a289-9075641ce6be.png)
&nbsp;&nbsp;&nbsp;&nbsp;
![image](https://user-images.githubusercontent.com/113395677/222940678-08786dca-7a06-4b8d-8e75-18ba340e4422.png)
&nbsp;&nbsp;&nbsp;&nbsp;
![image](https://user-images.githubusercontent.com/113395677/226063666-4c092d1b-0017-421c-9d99-901a53ae5b00.png)

Columns and small names options.

![image](https://user-images.githubusercontent.com/113395677/229323981-305f5725-00c3-439d-a431-a8ee919c032b.png)

## Build
Clone. Download [ImGui 1.8](https://github.com/ocornut/imgui/tree/v1.80). Extract the Imgui to the cloned directory. Run the bat file. The DLL will be in the out directory.
Due to some parts being closed source the resulting DLL will not work correctly. 

[msvc](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) x64:
```
build_win32.bat
```

## Install:
make sure you have [arcdps](https://www.deltaconnected.com/arcdps/)

move the know_thy_enemy.dll([releases](../../releases)) to the folder where the gw2 exe is or the bin64 folder (its in the same folder as your gw2 exe)

then in-game open the arcdps options window (alt-shift-t)

Under the "Extensions" tab, you will see a sub page called Know thy enemy, where you can enable and disable the extension.


## Use:
Right click the window to toggle between a history of fights and the current instance

## Comments, concerns, and/or proclamations

post an [issue](../../issues)

hmu in this new discord https://discord.gg/6xDbA64rZc

or in-game at woodel.6318
