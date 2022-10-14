# Sensor Firmware

This folder includes UI source code which is useful and easy to implement.

The structure of all source code here is similar to a python class.
Each class initializes a thread that runs in parallel with other tasts.
The thread is written in POSIX.


## Application
Intialize a class by calling it and naming a variable.

``` C
handle_t object = Open_Object(args...);
```

Perform functions using methods:
	
``` C
object.Method(&object, args...);
```

Since C does not have an ability to perform a SELF struction within a class, the
address to the object needs to be input into the method along with any arguments.
