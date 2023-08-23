# no_system

A iOS/iPadOS drop-in system() replacement for built-in use-cases

## What does it do?

no_system provides a way to execute other utilities that have been modified to use no_system.
It's similar in principle to ios_system by Nicolas Holzschuch, though avoids going through dyld,
resulting in statically linked "programs" to be executed as if they were called through system().

## License

no_system is licensed under the BSD 3-clause license.
