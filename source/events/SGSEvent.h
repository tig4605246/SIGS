/*

    Name: Xu Xi-Ping
    Date: April 20,2017
    Last Update: April 20,2017
    Program statement: 
        In here, We define the struct and some general variables, parameters

        After that, we define some datalog format
    Edited:
        Adding uploadInfo for controlling use

*/

//printf with colors
//Example printf(RED"Error\n");

#ifndef COLORS

#define COLORS

#define NONE "\033[m"
#define RED "\033[0;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[0;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[0;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;33m"
#define LIGHT_GRAY "\033[0;37m"
#define WHITE "\033[1;37m"

#endif

//Purpose : print out the stored the error message and the error flag.
//Pre : 