# Demos for Hameg and Raspberry PI

This repo contains some trivial demos for an oscilloscope driven from the
audio o/p of a RasPi (2 or above recommended). 

The RasPi should be running a moderately recent Raspbian. You may need
to install `build-essential` and `libao`.

You will need to remove the AC decoupling caps from the audio circuit
so you can drive the output in a vaguely deterministic fashion. 

Then couple connect the two channels of your scope to the L and R
audio outputs, set X-Y mode, 100mV sensitivity.

Now you can type `make`, sit back and play pong or watch cubes or
spaceships float around your screen.

`pong` is intended to be played on two numeric keypads connected to
your pi via USB. It takes two arguments - the numbers of `/dev/hidraw`
devices to poll for the left and right player, respectively. The controls
are:

| 8 | Move up |
| 2 | Move down |
| 4 | Move in |
| 6 | Move out |

We don't do back-face culling yet, sadly.
