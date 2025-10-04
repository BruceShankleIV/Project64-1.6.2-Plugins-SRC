/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdl_input.h"
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_gamecontroller.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include "gui.h"

CRITICAL_SECTION critical_section; 
char dbpath[PATH_MAX];

int initialized = 0;
SDL_GameController *con = NULL;
int joy_inst = -1;

void try_init(void)
{
    EnterCriticalSection(&critical_section);

    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_GAMECONTROLLER))
    {
        initialized = 1;
    }

    LeaveCriticalSection(&critical_section);
}

void deinit(void)
{
    EnterCriticalSection(&critical_section);
    con_close();
    SDL_Quit();
    initialized = 0;
    LeaveCriticalSection(&critical_section);
}

void con_open(void)
{
    EnterCriticalSection(&critical_section);
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
    {
        if (SDL_IsGameController(i) && (con = SDL_GameControllerOpen(i)) != NULL)
        {
            SDL_Joystick *joy = SDL_GameControllerGetJoystick(con);
    
            joy_inst = SDL_JoystickInstanceID(joy);

            SDL_JoystickGUID guid = SDL_JoystickGetGUID(joy);
            char guidstr[33];
            SDL_JoystickGetGUIDString(guid, guidstr, sizeof(guidstr));

            char *mapping = SDL_GameControllerMapping(con);
            if (mapping != NULL) {
                SDL_free(mapping);
            } else {
                con_close();
                continue;
            }

            break;
        }
    }
    
    LeaveCriticalSection(&critical_section);
}

void con_close(void)
{
    EnterCriticalSection(&critical_section);
    SDL_GameControllerClose(con);
    con = NULL;
    joy_inst = -1;
    LeaveCriticalSection(&critical_section);
}

int16_t threshold(int16_t val, float cutoff)
{
    if (val < 0)
        return val >= -(cutoff * 32767) ? 0 : val;
    else
        return val <= (cutoff * 32767) ? 0 : val;
}

void scale_and_limit(int16_t *x, int16_t *y, float dz, float edge)
{
    // get abs value between 0 and 1 relative to deadzone and edge
    int16_t div = edge * 32767 - dz * 32767;
    if (div == 0) {
        return;
    }
    float fx = (abs(*x) - dz * 32767) / div;
    float fy = (abs(*y) - dz * 32767) / div;

    // out of range
    if (fx > 1.f) {
        fy = fy * (1.f / fx);
        fx = 1.f;
    }
    if (fy > 1.f) {
        fx = fx * (1.f / fy);
        fy = 1.f;
    }

    float sign_x = 0;
    float sign_y = 0;

    // deadzone
    if (*y != 0) {
        if (fy <= 0.f)
            fy = 0.f;
        else
            sign_y = abs(*y) / *y;
    }

    if (*x != 0) {
        if (fx <= 0.f)
            fx = 0.f;
        else 
            sign_x = abs(*x) / *x;
    }

    *x = sign_x * fx * 32767;
    *y = sign_y * fy * 32767;
}

int16_t sclamp(int16_t val, int16_t min, int16_t max)
{
    if (val <= min) return min;
    if (val >= max) return max;
    return val;
}

int16_t smin(int16_t val, int16_t min)
{
    if (val <= min) return min;
    return val;
}

int16_t smax(int16_t val, int16_t max)
{
    if (val >= max) return max;
    return val;
}

void con_get_inputs(inputs_t *i)
{
    EnterCriticalSection(&critical_section);
    if (!initialized)
    {
        try_init();
        if (!initialized) {
            LeaveCriticalSection(&critical_section);
            return;
        }
    }
    LeaveCriticalSection(&critical_section);

    SDL_Event e;
    while (SDL_PollEvent(&e))
        switch (e.type)
        {
        case SDL_CONTROLLERDEVICEADDED:
            if (con == NULL)
            {
                con_open();
            }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (e.cdevice.which == joy_inst)
            {
                con_close();
                con_open();
            }
            break;
        }

    EnterCriticalSection(&critical_section);
    if (con != NULL)
        con_write_inputs(i);
    LeaveCriticalSection(&critical_section);
}

static inline uint8_t con_get_but(SDL_GameControllerButton b)
{
    return SDL_GameControllerGetButton(con, b);
}

static inline int16_t con_get_axis(SDL_GameControllerAxis a)
{
    return sclamp(SDL_GameControllerGetAxis(con, a), -32767, 32767);
}

void con_write_inputs(inputs_t *i)
{
    i->a      = con_get_but(SDL_CONTROLLER_BUTTON_A);
    i->b      = con_get_but(SDL_CONTROLLER_BUTTON_B);
    i->x      = con_get_but(SDL_CONTROLLER_BUTTON_X);
    i->y      = con_get_but(SDL_CONTROLLER_BUTTON_Y);
    i->back   = con_get_but(SDL_CONTROLLER_BUTTON_BACK);
    i->guide  = con_get_but(SDL_CONTROLLER_BUTTON_GUIDE);
    i->start  = con_get_but(SDL_CONTROLLER_BUTTON_START);
    i->lstick = con_get_but(SDL_CONTROLLER_BUTTON_LEFTSTICK);
    i->rstick = con_get_but(SDL_CONTROLLER_BUTTON_RIGHTSTICK);
    i->lshoul = con_get_but(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    i->rshoul = con_get_but(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    i->dup    = con_get_but(SDL_CONTROLLER_BUTTON_DPAD_UP);
    i->ddown  = con_get_but(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    i->dleft  = con_get_but(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    i->dright = con_get_but(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

    i->alx    = con_get_axis(SDL_CONTROLLER_AXIS_LEFTX);
    i->aly    = con_get_axis(SDL_CONTROLLER_AXIS_LEFTY);
    i->arx    = con_get_axis(SDL_CONTROLLER_AXIS_RIGHTX);
    i->ary    = con_get_axis(SDL_CONTROLLER_AXIS_RIGHTY);
    i->altrig = con_get_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    i->artrig = con_get_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
}