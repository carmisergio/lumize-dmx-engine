import pygame, sys, random
from pygame.locals import QUIT
from ola.ClientWrapper import ClientWrapper 
import array
import threading
import os

# CONFIG VARIABLES #
WINDOWWIDTH = 500
WINDOWHEIGHT = 400
FPS = 120
BOARDWIDTH = 5
BOARDHEIGHT = 4
UNIVERSE_ID = 1
# CONFIG VARIABLES #

BOXWIDTH = WINDOWWIDTH / BOARDWIDTH
BOXHEIGHT = WINDOWHEIGHT / BOARDHEIGHT

font = None

def pygameUpdate(name):
    global FPSCLOCK
    while True:
        #print("secloop")
        #Handle pygame events
        for event in pygame.event.get():
            if event.type == QUIT:
                print ("Exiting")
                pygame.quit()
                os._exit(1)
        pygame.display.update()
        FPSCLOCK.tick(10)


def updateDisplay(data):
    global lightdata
    global font
    lightdata= data
    #Fill window
    DISPLAYSURF.fill((0, 0, 0))
    
    i = 0
    #Render light values
    for y, _ in enumerate(range(0, BOARDHEIGHT)): #For every line
        for x, _ in enumerate(range(0, BOARDWIDTH)): #For every square in that line
            
            data = lightdata[i]
            
            #Make sure maximum value is 255
            if data > 255:
                data = 255
            
            #Draw
            pygame.draw.rect(DISPLAYSURF, (data, data, data), (x * BOXWIDTH, y * BOXHEIGHT, BOXWIDTH, BOXHEIGHT))
            text = font.render(str(i + 1), True, (255, 0, 0))
            DISPLAYSURF.blit(text, (x * BOXWIDTH, y * BOXHEIGHT))
            
            i += 1 #Increment counter


    #Update display
    #print("Data comin' in HOT!")
    pygame.display.update()

#Main function
def main():
    global FPSCLOCK, DISPLAYSURF
    global font
    #Generate array with 512 0s
    lightdata = []
    for _ in range(0, 512):
        lightdata.append(0)

    
    #Initialize pygame + pygame clock
    pygame.init()
    FPSCLOCK = pygame.time.Clock()
    DISPLAYSURF = pygame.display.set_mode((WINDOWWIDTH, WINDOWHEIGHT))
    pygame.display.set_caption("Light Output Visualizer")
    font = pygame.font.Font(None, 25)

    x = threading.Thread(target=pygameUpdate, args=(1,))
    x.start()

    #Initialize Ola client
    wrapper = ClientWrapper()
    client = wrapper.Client()
    client.RegisterUniverse(UNIVERSE_ID, client.REGISTER, updateDisplay)
    wrapper.Run()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print ("Exiting")
        pygame.quit()
        os._exit(1)
