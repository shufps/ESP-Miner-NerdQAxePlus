***************************************************************
* Arduino PID Library - Version 1.2.3
* by David Forrest <drf5na@gmail.com>  2023-02-24
* by Brett Beauregard <br3ttb@gmail.com> brettbeauregard.com
*
* This Library is licensed under the MIT License
***************************************************************

 - For an ultra-detailed explanation of why the code is the way it is, please visit:
   http://brettbeauregard.com/blog/2011/04/improving-the-beginners-pid-introduction/

 - For function documentation see:  http://playground.arduino.cc/Code/PIDLibrary


This fork uses back calculation per [Astrom 1989](http://cse.lab.imtlucca.it/~bemporad/teaching/controllodigitale/pdf/Astrom-ACC89.pdf) to manage integral windup.

The back calculation should prevent integral windup be dynamically limiting the Intergrato to precent the Control Output
from exceeding the limits.  Large errors far outside of the proportional range (error > MaxOutput/kP) will produce MaxOutput and
inhibit integral growth.

Alternative libraries:

* PID -- Brett Beauregard's well documented original uses static limits on integral windup or PonM scheme
* QuickPID -- has several anti-windup forms
* PID_RT -- Has anti-windup but Proportional-on-measurement


See:
*  https://en.wikipedia.org/wiki/PID_controller#Integral_windup -- overview of Integral Windup & common solutions
*  https://en.wikipedia.org/wiki/Integral_windup -- overview of Integral Windup & common solutions
* https://www.cds.caltech.edu/~murray/courses/cds101/fa02/caltech/astrom-ch6.pdf -- Textbook chaper with windup and backcalculation
* https://controlguru.com/integral-reset-windup-jacketing-logic-and-the-velocity-pi-form/ -- good pics
*  https://homepages.laas.fr/lzaccari/preprints/ZackEJC09.pdf -- A comprehensive paper on more complicated anti-windup schemes than backcalculation
*  https://github.com/br3ttb/Arduino-PID-Library/pull/116 -- Pull request to the PID_v1 library
*  https://github.com/br3ttb/Arduino-PID-Library/issues/76
