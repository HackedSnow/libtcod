﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using libtcod;

namespace TestApp
{
    class Program
    {
        static void Main(string[] args)
        {
            TCODConsole.initRoot(80, 50, "Test - C#", false, TCOD_renderer_t.TCOD_RENDERER_SDL);
            TCODConsole.root.setForegroundColor(TCODColor.white);
            while (!TCODConsole.isWindowClosed())
            {
                TCODConsole.root.printLeft(3, 3, TCOD_bkgnd_flag_t.TCOD_BKGND_SET, "Hello World");
                TCODConsole.root.putCharEx(10, 10, '@', TCODColor.white, TCODColor.black);
                TCODConsole.flush();
                TCODConsole.checkForKeypress();
            }
        }
    }
}