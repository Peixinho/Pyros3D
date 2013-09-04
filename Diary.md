Pyros 3D

Project Started on: June 18, 2013

Why?

	I've started this project with the idea that most of the old code was good, but there where some problems/limitations that I wasn't feeling good about.
	One thing that I didn't like was the supersmartpointers, it always felt like a waste of time and resources, and in the end, it didn't save me from having some memory problems too, so these are some of my reasons to rewrite the egine. Try to make the engine more flexible and at the same time, fix those memory issues.

	On the other side, there was the performance issue. I think that the old engine was going well, but there is a part of me that doesn't stand the fact that it can't	handle many objects (even if they are not on the screen), and this new engine have that target too.

	Finally the last reason is simply because I wanna have a Deferred Rendering on the engine too. I mean, I want to have support for both Forward and Deferred, or both at the same time, or even anything else.
	What I had previously was very strictly aimed for forward rendering and trying to apply a different ways of rendering was almost impossible. Now I will try a new approach that I hope that will allow me to have different rendering pipelines.

	The name .. as always I've thought in many different ones, but ended up using "Pyros". It sounds strange probably but it has a special meaning for me (its kinda private joke for myself).

Main Goals:
	Remove SuperSmartPointers;
 	Redo most of stuff and avoid memory issues;
 	Performance boost
 	Allow Deferred Rendering or anything else
	Reuse many stuff from old Fishy3D (good stuff only I hope)

Diaries:

	1st - July 5, 2013:

		After 2 weeks of development, the engine has reached the state that I wished until now (but thought that would accomplish much sooner). I have a simple forward renderer working,
		but its not sorting anything yet or rendering only stuff that are on the scene, it just renders every mesh that is loaded into memory and have a component associated.
		The performance is at least as good as it was, but I think it is slightly better and since now it uses multithreading, it will only get better that the old engine when the number of objects increase in the screen (or in the scene, even if not visible).
		I removed the supersmartpointers and until now, not a single memory problem, the code seems more stable and clean, and I have a better clue what's happening under the hood (something that I didn't know previously).
		I have implemented a system that allow me also to have components with any type without having to write code on the interface, or in the scenegraph etc. On the other engine I would have to change the scenegraph so it could register the components. Now that happens but its done completely different, so each new type of component must have its own "sub engine".

	2nd - August 22, 2013:

		2 months after starting the project, I've reached the level that my previous version had, and plus. At this time, I've started implementing physics, so its on the same development phase that I was on the previous version when I decided to rewrite. But I've added some more features that the previous engine didn't have, so it now supports 1 Directional Lights Shadows (Cascade Shadow Mapping [4 Cascades Max]), 4 Point Light Shadows (CubeMap Shadow Maps) and 4 Spot Light Shadows. I was going to support Regular and GPU shadow maps, but nowadays, any GPU supports HW Shadows so I removed regular shadow maps from the engine. (Its always possible to implement over the engine, extending it)
		One thing is not implemented yet, like I had on the previous version, and that is mesh sorting, so for now, I can't sort opaque from translucid meshes (materials) but thats on a todo list, and I wanna go further, and implement even better sorts, to optimize performance.
	

	3rd - September, 2013:

		1 month after my last update on this file, I have 2 features implemented that worth to be mentioned. The forward renderer is faster than ever, and when I have sorting implemented, the performance will increase even more. My first achievment when rewriting this new version is complete, I have now a much better performance on screen.
		The other feature is finally, text rendering. Its now possible to load a font, create the rendering component and render to the screen.
		There was a lot of bug fixing and cleaning!

TODO List:
	On the rendering part I have to compare the rendering list with the gameobjects on the scene to render, and render only the ones that are on that list. - DONE
	Probably I can make a list for each scene to speed up. - DONE

	Try some sort stuff to boost rendering pipeline. - PARTLY DONE

	Work on the lighting part again, mainly is copy the old code and try to optimize, but most of the code is ready to go - DONE

	Work on shadows again, reimplment cascade shadows, and probably start working on other shadow types, or at least prepare engine to work on that later. - DONE

	Reimplement Physics as I had - DONE

	Redo Car Game

	Go Deferred!
