# Style guidelines
Because DobieStation is so new, it's important to enforce style guidelines to ensure that the code remains consistent in the distant future.

## General
If nothing else, follow the style of the file(s) you're modifying.

* Use ```/**comments**/``` for file-wide documentation, ```/*comments*/``` for multi-line comments, and ```//comments``` for everything else.
* Functions, local variables, class/struct members: ```snake_case```
* Classes and structs: ```PascalCase```
  * Exception: PS2-register structs are ```UPPER_SNAKE_CASE```

## Example
```
/**
Here's some nice documentation for the file.
The GS takes data from the VU1, VIF, and EE and makes a lot of graphics with it.
**/

/* 
The ChewToy struct is responsible for playing a chewing sound.
make_sound works through deep magic. Do not modify!
*/
struct ChewToy
{
    void make_sound();
};

//This is a super important I/O register for the Graphics Synthesizer, so the name is in UPPER_SNAKE_CASE.
struct IMPORTANT_GS_REG
{
    bool flag1, flag2;
};

//This function prints a bark to the console.
void good_boy()
{
    const char* bark_sound = "Bark!";
    printf("%s\n", bark_sound);
}
```
