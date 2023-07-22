![](https://media.tenor.com/g6nK3QsP8wkAAAAC/kdaeza-peepo.gif)

[July 21st] 

So basically the arcdev (I call them Humpty) made some changes to the arcdps API for WvW. These changes made by Humpty specifically remove agent instance IDs and their generic IDs from the API. The changes replaced the agent ID values with 0xFFFF and profession ID, respectfully. In order to circumvent the Humpty changes it is going to require some time, maybe a couple of weeks due to only having some free time on the weekends. Until then I can only suggest using an arcdps log parser that can count enemy combatants, do note large fights will cause significant lag when the log is written, will take up some space on your harddrive, will require tabbing out of the game, and are only shown after the log period has ended (normally once everyone in your squad is out of combat).

[July 19th] 

ArcDps dev cucked the WvW API for Addons **AGAIN**, so this is on hiatus till I reverse his code, Cheers

# Know-thy-enemy [WvW]
[![](https://img.shields.io/github/downloads/typedeck0/know-thy-enemy/total?style=plastic)](../../releases)

Tired of commanders saying "They're *twice* our size!"? Well now you can put a number on it!

Counts the amount and type of player enemies (that your squad hits or is hit by) in an arcdps fight instance (resets when arcdps does).

Counts the amount of enemies you hit (more tags = more bags).

![image](https://user-images.githubusercontent.com/113395677/222940654-ff55d512-85e5-42dc-a289-9075641ce6be.png)
&nbsp;&nbsp;&nbsp;&nbsp;
![image](https://user-images.githubusercontent.com/113395677/222940678-08786dca-7a06-4b8d-8e75-18ba340e4422.png)
&nbsp;&nbsp;&nbsp;&nbsp;
![image](https://user-images.githubusercontent.com/113395677/226063666-4c092d1b-0017-421c-9d99-901a53ae5b00.png)

Columns and small names options.

![image](https://user-images.githubusercontent.com/113395677/229323981-305f5725-00c3-439d-a431-a8ee919c032b.png)

## Build
Clone. Download [ImGui 1.8](https://github.com/ocornut/imgui/tree/v1.80). Extract the Imgui to the cloned directory. Run the bat file. The dll will be in the out directory.

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
