#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "keypad.c"
#include "lcd.c"
#include "rtc.c"
#include "eeprom.c"

/* Defines */
//stages codes to be used inside keypad ISR to distinguish where the user is currently (since every press carry the execution to the ISR
#define ON_MAIN_MENU 0 
#define ON_CREATE_NAME 1
#define ON_CREATE_NUMBER 2
#define ON_FIND_NAME 3
#define ON_DEL_NAME 4

/* Static Variables */

int stage; // used as indicator of where the user is ,SINCE KEYPAD ISR IS CALLED EVERY KEYPAD PUSH BUTTON  
char name[16]; // name to be entered by user ( at any choise )
char number[16]; // number to be entered by user ( at any choise )
/* "name" , "number" must be global , since every pressed button the ISR executed & then return to main & so on.. so every name & number entered are saved in them & then nulled so they can be used in next operation */ 
int input_index = 0; // keypad input array index
unsigned long int seconds; // variable holds seconds counter of RTC 
uint32_t contact[2]; // value to be written in EEPROM    
uint32_t contact_read[2]; // variable holds data to bread form EEPROM
uint32_t eeprom_address = 0; // EEPROM addressing index
char contact_names[16][16] = {0}; // array hold contact names while writing in EEPROM ( created due to bugs while reading from EEPROM )
char contact_numbers[16][16] = {0}; // array hold contact numbers while writing in EEPROM ( created due to bugs while reading from EEPROM )
int contacts_index = 0; // index for above arrays
uint32_t address_to_del = 128; // EEPROM address to delete when user delete contact  , 128 max value ( 16 contact ( 8 bytes each ) ) 


/* Main Functions declaration */ 
void welcome_screen(void);    // print "MILESTONE 4" to LCD
void init(void); // display initialization menu 
void PortCIntHandler(void);  // keypad ISR
void enable_int(void); // keypad interrupt enabler
void handle_error(void); // Error handler
void ask_for_name(void); // ask for name input on LCD
void ask_for_number(void); // ask for number input on lcd
void null_globals(void); // a function called after eeach operations which nulls global variable to be used again in next ops
char* find_contact(void); // search for input name contact & return its number (void argument because "name" is global variable , no need to pass it) 

#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
    while(1);
}
#endif




int main(void)
{
  
    /* initializations */
    keypad_init();
    enable_int();
    lcd_init();
    rtc_init();
    eeprom_init();
    welcome_screen();
    init();
    
    

    
    /* program loop */
    while(1)
    {
      SysCtlSleep(); // for low power consumption until interrupt 
    }
}
    
    
   

void enable_int()
{   
   GPIOIntTypeSet(KEYPAD_COL_PORT,KEYPAD_COL_PINS,GPIO_LOW_LEVEL); // set interrupts to FALLING EDGE ( as keypad charachter is 0 when pressed ) GPIO_FALLING_EDGE
   GPIOIntRegister(KEYPAD_COL_PORT, PortCIntHandler); // set ISR as "PortCInHandler"
   GPIOIntEnable(KEYPAD_COL_PORT,KEYPAD_COL_PINS); // enable the interrupts on keypad colomn pins
}

void PortCIntHandler()
{
  GPIOIntClear(KEYPAD_COL_PORT,KEYPAD_COL_PINS); // clear interrupt  
  char key = read_keypad(); // read pressed character
  if (key == 0) return; // if nothing pressed currently , leave & wait for next interrupt (pressed button)
  delay_ms(150); // delay 150 milliseconds
  
  if (stage == ON_MAIN_MENU) // if the user currently in " Main Menu "
  {
    switch(key)
    {
    case '1' : stage = ON_CREATE_NAME; ask_for_name();  break; // shift stage to "Create" & ask for name
    case '2' : stage = ON_FIND_NAME; ask_for_name(); break; // shift stage to "Find" & ask for name 
    case '3' : stage = ON_DEL_NAME; ask_for_name(); break; // shift stage to "Delete" & ask for name
    }
    
    return; // return to low level power consumption until keypad pressed (interrupt)
  }
  
  if (stage == ON_CREATE_NAME) //  if the user currently in " Create.Name"
  {
    if (key == '*') // if user done with input (* = OK)
    {
      input_index = 0; // 0 the index to be used again in nexxt operations
      stage = ON_CREATE_NUMBER; // shift to "Create.Number" stage
      ask_for_number(); // ask for number input on LCD
    }
    
    else  // if user still typing input name
    {
      name[input_index] = key; // save pressed char in "name" gloabal variable
      input_index++; // increment indexing variable
      lcd_data(key); // print pressed char
    }
    return; // return to low level power consumption until keypad pressed (interrupt)
  }
  
  if (stage == ON_CREATE_NUMBER) // if the user currently in " Create.Number"
  {
    if (key == '*') // if user done with input number
    {      
      strcpy(contact_names[contacts_index],name); // copy "name" global variable to contacts array
      strcpy(contact_numbers[contacts_index],number); // copy "number" global variable to contacts array
      contact[0] = (uint32_t)contact_names[contacts_index]; // type cast to uint32 from char* name to be saved in EEPROM
      contact[1] = (uint32_t)contact_numbers[contacts_index]; // type cast to uint32 from char* number to be saved in EEPROM
      contacts_index++; // increment contacts indexing
      /* Contacts array may look unneccessary , but it is crucial , as some issues existed while reading from EEPROM , & this was the solution */
      uint32_t success = EEPROMProgram(contact, eeprom_address, sizeof(contact)); // write to EEPROM & return status of writeing
      null_globals(); // null the globals used to be used again in next operation
      eeprom_address += 8; // increment addressing of EEPROM by 8 bytes ( 4 byte name , 4 byte number )
      lcd_reset(); // reset LCD
      if (success == 0) // if status of witing is succuess
      {
        lcd_write_string("[*]  SAVED  [*]"); 
      }
      else // if status of writing is failure
      {
        lcd_write_string("[X]  ERROR  [X]");
      }
      delay_ms(2000); // delay 2 seconds for user to see the message
      init(); // back to main menu
    }
    
    else // if user still giving input & not pressed ok (*)
    {
      number[input_index] = key; // save the presse key in "name" global
      input_index++; // increment input indexing
      lcd_data(key); // print pressed char to LCD
    }
    
    
    
    return; // return to low level power consumption until keypad pressed (interrupt)
  }
  
  if (stage == ON_FIND_NAME) // if user currently in "Find" stage
  {
    
    if(key == '*') // if user done writing name to search for 
    {
      char *found_number = find_contact(); // find_contact() will search for the number of "name" global variable (which is the last input)
      
      if (found_number) // if found number not equal '\0' , then the function returned a number
      {
        
        lcd_reset(); // reset LCD
        lcd_write_string(name); // print name on LCD
        lcd_cmd(0xC0); // shift to next line on LCD to write the returned number
        lcd_write_string(found_number); // write returned number
        null_globals(); // null "name" & "number" global variables to be used again
        delay_ms(3000); // delay until user see the number
        
      }
      
      else // if find_contact() didn't return a number , then contact is not found in EEPROM
      {
        null_globals(); // null the globals for next operations
        lcd_reset(); // reset LCD
        lcd_write_string("[X]NOT FOUND[X]"); // write "NOT FOUND" on lcd
        delay_ms(2000); // delay until user see the message
      }
      
      init(); // stage finished (either found number or not) , back to main menu
      return; // return to low level power consumption until keypad pressed (interrupt)
    }
    
    else // if user still giving input name & not pressed * (OK)
    {
      name[input_index] = key; // save pressed key to "name" global
      input_index++; // increment indexing
      lcd_data(key); // print pressed char
    }
      
    
    // no init() as stage can still no be finished and user still giving interrupts (pressing keypad)
    return;  // return to low level power consumption until keypad pressed (interrupt)
  }
  
  
  if (stage == ON_DEL_NAME) // if user currently in "DELETE" stage
  {
    if (key == '*') // if user done writing name
    {
      if(! find_contact()) // if contact doesn't exist (returns null)
        /* THE TRUE TARGET OF find_contact() HERE IS _IF CONTACT FOUND_ , THE ADDRESS OF THIS CONTACT WILL BE PUT IN "address_to_del" GLOBAL */
      {
        null_globals(); // null gloabal variables to be used again
        lcd_reset(); // reset LCD
        lcd_write_string("[X]NOT FOUND[X]"); // tell user that contact not found (unable to delete)
      }
      
      else if(address_to_del != 128) // if the "address_to_del" is not the initial value , & find_contact() returned a number , then CONTACT EXIST & its address is in "address_to_del"
      {
        uint32_t trash[2] = {0}; // a garbage (null) value to overwrite contact address in EEPROM
        EEPROMProgram(trash,address_to_del,sizeof(trash)); // write the null value to the address of targeted contact
        lcd_reset(); // reset LCD
        lcd_write_string("[*] DELETED [*]"); // tell the user that contact deleted sucessfully
      }
      delay_ms(2000); // delay until user see the message
      init(); // back to main menu
      return; // return to low level power consumption until keypad pressed (interrupt)

    }
    
    else // else if user still giving input (not pressed *(OK) )
    {
      name[input_index] = key; // save the key in "name" global 
      input_index++; // increment indexing
      lcd_data(key); // print pressed key on LCD
    }
  } 
}




/* Functions */

void welcome_screen()
{
   lcd_write_string("  MILESTONE 4  "); // print string to LCD
   delay_seconds(5);
}

void handle_error()
{
      lcd_write_string("ERROR");
}

void init() // main menu display
{
  lcd_reset(); // reset LCD
  lcd_write_string("1-CREATE,2-FIND"); // Display options
  lcd_cmd(0xC0); 
  lcd_write_string("3-DEL : ");
  stage = ON_MAIN_MENU; // shift stage to "Main Menu" whatever the previes stage is 
}

void ask_for_name() // display "Name : " for user to enter name
{
  lcd_reset(); // reset LCD
  lcd_write_string("NAME : (*=OK)"); // tell the user to press (*) for OK
  lcd_cmd(0xC0); // next line to wait for input
}

void ask_for_number() // same as ask_for_name()
{
  lcd_reset();
  lcd_write_string("Number : (*=OK)");
  lcd_cmd(0xC0);
}

void null_globals() // nulls the global variables ( "name , "number" ) as they are used for every input name & input number
{
  for (int counter=0;counter<16;counter++)
  {
    name[counter] = '\0';
    number[counter] = '\0';
  }
  input_index = 0;
}

char* find_contact() // search if the "name" global variable (last name input) exist in EEPROM
{
      uint32_t read_address; // address indexing variable 
      char *found_number = {0}; // the initial value of find number ('\0' so if not found still null)
      for (read_address=0;read_address<eeprom_address;read_address+=8) // traverse eerpom 8 bytes by 8 bytes
      {
        EEPROMRead(contact_read,read_address,sizeof(contact_read)); // read the value in "contact_read" global
        char *x = (char *)contact_read[0]; // type cast the first 4 bytes (name) into char*
        if (!strcmp(x,name)) // if the name to find is the same as name in current EEPROM address
        {
          address_to_del = read_address; // set the global variable "address_to_del" with the current EEPROM address
          /* address_to_del is globale because this funtion used when finding contact & when deleting contact , to get the address of a contact to delete a global variable is used */
          found_number = (char *)contact_read[1]; // the number of contact is the secont 4 bytes read from EEPROM
          break; 
        }
      }
      return found_number; // returns found number as "NULL" or as a string (to know tf the number is found or not)
      
}


