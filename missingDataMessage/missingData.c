#include <SDL/SDL.h>

#define SCREEN_W	320
#define SCREEN_H	240
#define SCREEN_BPP	16

int exitProgram = 0;

SDL_Surface *screen;
SDL_Surface *image;
SDL_Event event;

int initSDL();
void input();
SDL_Surface *loadImage(const char *filename);
void draw();


int initSDL()
{
	if(SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "ERROR: (initSDL) Unable to initialize SDL 1.2: %s.\n", SDL_GetError());
		return 1;
	}

	SDL_ShowCursor(SDL_DISABLE);
	screen = SDL_SetVideoMode(SCREEN_W, SCREEN_H, SCREEN_BPP, SDL_HWSURFACE|SDL_DOUBLEBUF);
	if(screen == NULL)
	{
		fprintf(stderr, "ERROR: (initSDL) Failed to set video mode.\n");
		return 1;
	}

	image = loadImage("nodata.bmp");

	return 0;
}

void input()
{
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_QUIT:
				exitProgram = 1;
			break;
			case SDL_KEYDOWN:
				switch(event.key.keysym.sym)
				{
					//case SDLK_LCTRL:	// A
					case SDLK_RETURN:	// START
						exitProgram = 1;
					break;
				}
			break;

			default:
			break;
		}
	}
}

SDL_Surface *loadImage(const char *filename)
{
	SDL_Surface *loadedImage;
	SDL_Surface *optimizedImage;

	if(filename == NULL)
	{
		fprintf(stderr, "ERROR: (loadImageSDL) Filename is NULL.\n");
		return NULL;
	}

	loadedImage = SDL_LoadBMP(filename);
	if(loadedImage == NULL)
	{
		fprintf(stderr, "(loadImageSDL) Failed to load file: \"%s\".\n", filename);
		return NULL;
	}
	optimizedImage = SDL_DisplayFormat(loadedImage);
	SDL_FreeSurface(loadedImage);
	if(optimizedImage == NULL)
	{
		fprintf(stderr, "(loadImageSDL) Failed to optimize image: \"%s\".\n", filename);
		return NULL;
	}

	// set the transparency to magenta
	Uint32 colorKey = SDL_MapRGB(optimizedImage->format, 255, 0, 255);
	SDL_SetColorKey(optimizedImage, SDL_SRCCOLORKEY, colorKey);

	return optimizedImage;
}

void draw()
{
	SDL_BlitSurface(image, NULL, screen, NULL);
	SDL_Flip(screen);
}

// main function
int main()
{
	if(initSDL())
	{
		SDL_Quit();
		return 1;
	}

	draw();

	while(!exitProgram)
	{
		input();
	}

	SDL_free(image);
	SDL_Quit();

	return 0;
}
