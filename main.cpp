#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <SFML/Graphics.hpp>
#include <fstream>

using namespace std;

#define MAX_MEM 4096
#define MAX_REG 16


#define MAX_STACK 16
#define MAX_KEY 16
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define MAX_FONT 16
#define FONT_SIZE 5

#define MEM_START 0x200
#define NB_MASK_1 0xF000
#define NB_MASK_2 0x0F00
#define NB_MASK_3 0x00F0
#define NB_MASK_4 0x000F

const uint8_t font [MAX_FONT][FONT_SIZE]= {
    {0xF0, 0x90, 0x90, 0x90, 0xF0},
    {0x20, 0x60, 0x20, 0x20, 0x70},
    {0xF0, 0x10, 0xF0, 0x80, 0xF0},
    {0xF0, 0x10, 0xF0, 0x10, 0xF0},
    {0x90, 0x90, 0xF0, 0x10, 0x10},
    {0xF0, 0x80, 0xF0, 0x10, 0xF0},
    {0xF0, 0x80, 0xF0, 0x90, 0xF0},
    {0xF0, 0x10, 0x20, 0x40, 0x40},
    {0xF0, 0x90, 0xF0, 0x90, 0xF0},
    {0xF0, 0x90, 0xF0, 0x10, 0xF0},
    {0xF0, 0x90, 0xF0, 0x90, 0x90},
    {0xE0, 0x90, 0xE0, 0x90, 0xE0},
    {0xF0, 0x80, 0x80, 0x80, 0xF0},
    {0xE0, 0x90, 0x90, 0x90, 0xE0},
    {0xF0, 0x80, 0xF0, 0x80, 0xF0},
    {0xF0, 0x80, 0xF0, 0x80, 0x80}
};

uint8_t * mem = new uint8_t[MAX_MEM];
uint8_t * registers = new uint8_t[MAX_REG];
uint8_t * soundTimer = new uint8_t;
uint8_t * delayTimer = new uint8_t;
uint16_t * stackPointer = new uint16_t;
uint16_t * programCounter = new uint16_t;
uint16_t * stack = new uint16_t[MAX_STACK];
uint16_t * I = new uint16_t;
bool * keyboard = new bool[MAX_KEY];
uint8_t * display = new uint8_t[DISPLAY_WIDTH * DISPLAY_HEIGHT];
bool * drawflag = new bool;
char defaultRom[] = "MAZE";

sf::RenderWindow window(sf::VideoMode(640, 640), "Chip-8");

void load (char* file) {    
    ifstream f;
    f.open(file);
    f.read((char*)(mem+MEM_START), MAX_MEM - MEM_START);
}

void loadFont (void) {
    for (int i = 0; i < MAX_FONT; i++) {
	for (int j = 0; j < FONT_SIZE; j++) {
	    mem[i*FONT_SIZE + j] = font[i][j];
	}
    }
}

void init (char* rom) {
    
    //init drawflag
    *drawflag = false;

    //init indexes
    *programCounter = MEM_START;
    *stackPointer = 0;

    //initialise display
    for (int i = 0; i < DISPLAY_HEIGHT * DISPLAY_WIDTH; i++) {
	display[i] = 0;
    }

    //init mem
    for (int i = 0; i < MAX_MEM; i++) {
	mem[i] = 0;
    }

    //init registers
    for (int i = 0; i < MAX_REG; i++) {
	registers[i] = 0;
    }

    //init stack
    for (int i = 0; i < MAX_STACK; i++) {
	stack[i] = 0;
    }

    loadFont();
    load(rom);

    srand(time(NULL));
    
}

uint16_t getOpcode (int index) {
    uint16_t opCode = (mem[index] << 8) | mem[index + 1];
    return opCode;
}

//clear display
void ex_00E0(){
    for (int i = 0; i < DISPLAY_HEIGHT * DISPLAY_WIDTH; i++) {
	display[i] = 0;
    }
    *drawflag = true;
    *programCounter += 2;
}

//return from subroutine
void ex_00EE(){
    *programCounter = stack[(*stackPointer)--];
    *programCounter += 2;
}

//jump to location nnn
void ex_1nnn(uint16_t opCode){
    *programCounter = opCode & (0x0FFF);
}

//call subroutine at nnn
void ex_2nnn(uint16_t opCode){
    stack[++(*stackPointer)] = *programCounter;
    *programCounter = opCode & (0x0FFF);
}

//skip next instruction if register x is equal to kk
void ex_3xkk(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t kk = opCode & (0x00FF);
    if (registers[x] == kk) *programCounter += 2;
    *programCounter += 2;
}

//skip next instruction if register x is not equal to kk
void ex_4xkk(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t kk = opCode & (0x00FF);
    if (registers[x] != kk) *programCounter += 2;
    *programCounter += 2;
}

//skip next instruction if register x is equal to register y
void ex_5xy0(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t y = (opCode & NB_MASK_3) >> 4;
    if (registers[x] == registers[y]) *programCounter += 2;
    *programCounter += 2;
}

//put the value kk into register x
void ex_6xkk(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t kk = opCode & (NB_MASK_3 + NB_MASK_4);
    registers[x] = kk;
    *programCounter += 2;
}

//adds the value in register x to kk and stores result in register x
void ex_7xkk(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    registers[x] += opCode & (NB_MASK_3 + NB_MASK_4);
    *programCounter += 2;
}

//stores value in register y in register x
void ex_8xy0(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t y = (opCode & NB_MASK_3) >> 4;
    registers[x] = registers[y];
    *programCounter += 2;
}

//sets register x to register x or register y
void ex_8xy1(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t y = (opCode & NB_MASK_3) >> 4;
    registers[x] = registers[x] | registers[y];
    *programCounter += 2;
}

//sets register x to register x and register y
void ex_8xy2(uint16_t opCode){    
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t y = (opCode & NB_MASK_3) >> 4;
    registers[x] = registers[x] & registers[y];
    *programCounter += 2;
}

//sets register x to register x xor register y
void ex_8xy3(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t y = (opCode & NB_MASK_3) >> 4;
    registers[x] = registers[x] ^ registers[y];
    *programCounter += 2;
}

//adds registers x and y, if the result is > 255 register F is set to 1, 0 otherwise
//the lowest two nibbles are stored in register x
void ex_8xy4(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t y = (opCode & NB_MASK_3) >> 4;
    registers[0x000F] = (registers[x] + registers[y]) > 255;
    registers[x] = (registers[x] + registers[y]) & (0x00FF);
    *programCounter += 2;
}

//if register x > register y register F is set to 1, otherwise 0
//register x is set to register x - register y
void ex_8xy5(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t y = (opCode & NB_MASK_3) >> 4;
    registers[0x000F] = registers[x] > registers[y];
    registers[x] -= registers[y];
    *programCounter += 2;
}

//if register x & 0x0001, register F is set to 1 otherwise 0
//register x is set to register x >> 1
void ex_8xy6(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    registers[0x000F] = registers[x] & 0x0001;
    registers[x] >>= 1;
    *programCounter += 2;
}

//if register y > register x register F is set to 1 otherwise 0,
//register x is set to register y - register x 
void ex_8xy7(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t y = (opCode & NB_MASK_3) >> 4;
    registers[0x000F] = registers[y] > registers[x];
    registers[x] = registers[y] - registers[x];
    *programCounter += 2;
}

//if register x & 0x8000 != 0 register F is set to 1 otherwise 0
//register x is set to register x * 2
void ex_8xyE(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    registers[0x000F] = ((registers[x] & 0x80) != 0);
    registers[x] <<= 1;
    *programCounter += 2;
}

//skip next instruction if registers x and y are not equal
void ex_9xy0(uint16_t opCode){
    uint8_t x = (opCode & NB_MASK_2) >> 8;
    uint8_t y = (opCode & NB_MASK_3) >> 4;
    if (registers[x] != registers[y]) *programCounter += 2;
    *programCounter += 2;
}

//register I is set to nnn
void ex_Annn(uint16_t opCode){
    *I = opCode & (NB_MASK_2 + NB_MASK_3 + NB_MASK_4);
    *programCounter += 2;
}

//jump to address nnn + register 0
void ex_Bnnn(uint16_t opCode){
    *programCounter = opCode & (0x0FFF);
    *programCounter += registers[0];    
}

//register x is set to a random byte & kk
void ex_Cxkk(uint16_t opCode){    
    uint16_t x = (opCode & NB_MASK_2) >> 8;
    registers[x] = (opCode & 0x00FF) & (rand() % 256);
    *programCounter += 2;
}

//draw n byte sprite from I onwards at positions determined by registers x and y
//register F is set if there is a collision
void ex_Dxyn(uint16_t opCode){
    uint16_t x = registers[(opCode & NB_MASK_2) >> 8] % DISPLAY_WIDTH;
    uint16_t y = registers[(opCode & NB_MASK_3) >> 4] % DISPLAY_HEIGHT;
    uint16_t n = (opCode & NB_MASK_4);

    uint8_t * sprite = new uint8_t [n];
    for (int i = 0; i < n; i++) {
	sprite[i] = mem[*I + i];
    }

    registers[0xF] = 0;
    
    for (int byte = 0; byte < n; byte++) {
	for (int bit = 0; bit < 8; bit++) {
	    int xc = (x + bit) % DISPLAY_WIDTH;
	    int yc = (y + byte) % DISPLAY_HEIGHT;
	    if ((sprite[byte] & (0x80 >> bit)) != 0) {
		if (display[yc * DISPLAY_WIDTH + xc]) {
		    registers[0xF] = 1;
		}
		
		display[yc * DISPLAY_WIDTH + xc] ^= 1;
	    }
	}
    }
    
    *drawflag = true;
    *programCounter += 2;
}

//skips next instruction if key code in register x is pressed
void ex_Ex9E(uint16_t opCode){
    uint16_t x = (opCode & NB_MASK_2) >> 8;
    if (keyboard[registers[x]]) *programCounter += 2;
    *programCounter += 2;
}

//skips next instruction if key code in register x is not pressed
void ex_ExA1(uint16_t opCode){
    uint16_t x = (opCode & NB_MASK_2) >>8;
    if (!keyboard[registers[x]]) *programCounter += 2;
    *programCounter += 2;
}

//set register x to the value of the delaytimer
void ex_Fx07(uint16_t opCode){    
    uint16_t x = (opCode & NB_MASK_2) >> 8;
    registers[x] = *delayTimer;
    *programCounter += 2;
}

//wait for key press then stores key code in register x
void ex_Fx0A(uint16_t opCode){
    uint16_t x = (opCode & NB_MASK_2) >> 8;
    bool done = false;
    for (uint8_t i = 0; i < MAX_KEY; i++) {
	if (keyboard[i]) {
	    done = true;
	    registers[x] = i;
	}
    }
    if (done) *programCounter += 2;
}

//sets delay timer to value in register x
void ex_Fx15(uint16_t opCode){
    uint16_t x = (opCode & NB_MASK_2) >> 8;
    *delayTimer = registers[x];
    *programCounter += 2;
}

//set sound timer to value in register x
void ex_Fx18(uint16_t opCode){
    uint16_t x = (opCode & NB_MASK_2) >> 8;
    *soundTimer = registers[x];
    *programCounter += 2;
}

//set I to I + register x
void ex_Fx1E(uint16_t opCode){    
    uint16_t x = (opCode & NB_MASK_2) >> 8;
    *I += registers[x];
    *programCounter += 2;
}

//sets I to the location of the charatcer sprite for the character in register x
void ex_Fx29(uint16_t opCode){
    uint16_t x = (opCode & NB_MASK_2) >> 8;
    *I = FONT_SIZE * registers[x];
    *programCounter += 2;
}

//hundreds value of register x
//is stored at address is stored in I
//tens and ones stored in following addresses
void ex_Fx33(uint16_t opCode){
    uint16_t x = (opCode & NB_MASK_2) >> 8;
    mem[*I] = (uint8_t)(registers[x]/100);
    mem[*I + 1] = (uint8_t)((registers[x] % 100)/10);
    mem[*I + 2] = (uint8_t)((registers[x] % 100)%10);
    *programCounter += 2;
}

//stores registers 0 through x in memory starting at address stored in I
void ex_Fx55(uint16_t opCode){
    uint16_t x = (opCode & NB_MASK_2) >> 8;
    for (int i = 0; i <= x; i++) {
	mem[*I + i] = registers[i];
    }    
    *programCounter += 2;
}

//loads values from memory into registers 0 through x starting at address I
void ex_Fx65(uint16_t opCode){
    uint16_t x = (opCode & NB_MASK_2) >> 8;
    for (int i = 0; i <= x; i++) {
	registers[i] = mem[*I + i];
    }    
    *programCounter += 2;
}

void executeOpcode (uint16_t opCode) {
    cout << "Executing: " << hex << opCode << endl;
    
    uint16_t nib_1 = opCode & NB_MASK_1;
    uint16_t nib_3 = opCode & NB_MASK_3;
    uint16_t nib_4 = opCode & NB_MASK_4;
    
    switch (nib_1) {
	
    case 0x0000:
	switch (opCode) {

	case 0x00E0:
	    ex_00E0();
	    break;

	case 0x00EE:
	    ex_00EE();
	    break;
	    
	}
	
	break;
	
    case 0x1000:
	ex_1nnn(opCode);
	break;
	
    case 0x2000:
	ex_2nnn(opCode);
	break;
	
    case 0x3000:
	ex_3xkk(opCode);
	break;
    
    case 0x4000:
	ex_4xkk(opCode);
	break;
    
    case 0x5000:
	ex_5xy0(opCode);
	break;
    
    case 0x6000:
	ex_6xkk(opCode);
	break;
	
    case 0x7000:
	ex_7xkk(opCode);
	break;
    
    case 0x8000:
        
	switch (nib_4) {
	    
	case 0x0000:
	    ex_8xy0(opCode);
	    break;

	case 0x0001:
	    ex_8xy1(opCode);
	    break;

	case 0x0002:
	    ex_8xy2(opCode);
	    break;

	case 0x0003:
	    ex_8xy3(opCode);
	    break;

	case 0x0004:
	    ex_8xy4(opCode);
	    break;

	case 0x0005:
	    ex_8xy5(opCode);
	    break;

	case 0x0006:
	    ex_8xy6(opCode);
	    break;

	case 0x0007:
	    ex_8xy7(opCode);
	    break;

	case 0x000E:
	    ex_8xyE(opCode);
	    break;
	    
	}
	
	break;
	    
    case 0x9000:
	ex_9xy0(opCode);
	break;
    
    case 0xA000:
	ex_Annn(opCode);
	break;
    
    case 0xB000:
	ex_Bnnn(opCode);
	break;
    
    case 0xC000:
	ex_Cxkk(opCode);
	break;
    
    case 0xD000:
	ex_Dxyn(opCode);
	break;
    
    case 0xE000:

	switch (nib_4) {

	case 0x000E:
	    ex_Ex9E(opCode);
	    break;

	case 0x0001:
	    ex_ExA1(opCode);
	    break;
	    
	}
	
	break;
    
    case 0xF000:

	switch (nib_3 + nib_4) {

	case 0x0007:
	    ex_Fx07(opCode);
	    break;
	    
	case 0x000A:
	    ex_Fx0A(opCode);
	    break;

	case 0x0015:
	    ex_Fx15(opCode);
	    break;

	case 0x0018:
	    ex_Fx18(opCode);
	    break;

	case 0x001E:
	    ex_Fx1E(opCode);
	    break;

	case 0x0029:
	    ex_Fx29(opCode);
	    break;

	case 0x0033:
	    ex_Fx33(opCode);
	    break;

	case 0x0055:
	    ex_Fx55(opCode);
	    break;

	case 0x0065:
	    ex_Fx65(opCode);
	    break;

	}
	
	break;
    
    }
}

void updateKeys () {
    /*
      0-1, 8-a
      1-2, 9-s
      2-3, A-d
      3-4, B-f
      4-q, C-z
      5-w, D-x
      6-e, E-c
      7-r, F-v
    */

    keyboard[0x0] = sf::Keyboard::isKeyPressed(sf::Keyboard::Num1);
    keyboard[0x1] = sf::Keyboard::isKeyPressed(sf::Keyboard::Num2);
    keyboard[0x2] = sf::Keyboard::isKeyPressed(sf::Keyboard::Num3);
    keyboard[0x3] = sf::Keyboard::isKeyPressed(sf::Keyboard::Num4);
    keyboard[0x4] = sf::Keyboard::isKeyPressed(sf::Keyboard::Q);
    keyboard[0x5] = sf::Keyboard::isKeyPressed(sf::Keyboard::W);
    keyboard[0x6] = sf::Keyboard::isKeyPressed(sf::Keyboard::E);
    keyboard[0x7] = sf::Keyboard::isKeyPressed(sf::Keyboard::R);
    keyboard[0x8] = sf::Keyboard::isKeyPressed(sf::Keyboard::A);
    keyboard[0x9] = sf::Keyboard::isKeyPressed(sf::Keyboard::S);
    keyboard[0xA] = sf::Keyboard::isKeyPressed(sf::Keyboard::D);
    keyboard[0xB] = sf::Keyboard::isKeyPressed(sf::Keyboard::F);
    keyboard[0xC] = sf::Keyboard::isKeyPressed(sf::Keyboard::Z);
    keyboard[0xD] = sf::Keyboard::isKeyPressed(sf::Keyboard::X);
    keyboard[0xE] = sf::Keyboard::isKeyPressed(sf::Keyboard::C);
    keyboard[0xF] = sf::Keyboard::isKeyPressed(sf::Keyboard::V);
       
}

int main (int argc, char* argv[]) {

    if (argc > 1) {
	init(argv[1]);
    }else{
	init(defaultRom);
    }
    while (*programCounter <= MAX_MEM && window.isOpen())
    {
	updateKeys();
	executeOpcode(getOpcode(*programCounter));

        sf::Event event;
        while (window.pollEvent(event))
        {
	    
            if (event.type == sf::Event::Closed)
                window.close();
        }

	if (*drawflag) {

	    // for (int i = 0; i < DISPLAY_HEIGHT; i++) {
	    // 	for (int j = 0; j < DISPLAY_WIDTH; j++) {
	    // 	    cout << 1*display[i*DISPLAY_WIDTH + j];
	    // 	}
	    // 	cout << endl;
	    // }
	    
	    for (int i = 0; i < DISPLAY_HEIGHT; i++) {
		for (int j = 0; j < DISPLAY_WIDTH; j++) {
		    sf::RectangleShape rectangle(sf::Vector2f(window.getSize().x / DISPLAY_WIDTH,
							      window.getSize().y / DISPLAY_HEIGHT));
		    rectangle.setPosition(j * (window.getSize().x / DISPLAY_WIDTH),
					  i * (window.getSize().y / DISPLAY_HEIGHT));
		    rectangle.setFillColor(sf::Color::Black);
		    if (display[i * DISPLAY_WIDTH + j]) {			
			rectangle.setFillColor(sf::Color::White);
		    }
		    window.draw(rectangle);
		}
	    }
	    
	    window.display();
	}

	if (*delayTimer > 0) *delayTimer -= 1;
	if (*soundTimer > 0) *soundTimer -= 1;
	*drawflag = false;
	

	cout << "Program counter: " << hex << 1*(*programCounter) << endl;
	for (int i = 0; i < MAX_REG; i++) {
	    cout << "reg[" << i << "]: " << hex << 1*registers[i] << endl; 
	}
	cout << "reg[I]: " << hex << *I << endl;
	//cin.get();
	//usleep(5000);
    }
    
    return 0;
}

/*
  8e34
  22d4
  a2f2
  fe33
  f265
  f129
  6414
  6500
  
*/
