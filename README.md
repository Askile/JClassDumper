**JClassDumper** is a Windows DLL project designed to scan the memory of the any java process, locate Java .class files by their magic number (CA FE BA BE), and dump them to disk

It uses pattern scanning with configurable parameters ot identify Java class files in memory and extract them into .class files for analysis or reverse engineering purposes.

```json
{
    "pattern": "CA FE BA BE",
    "header_size": 4
}
```

- pattern - the full sequence of bytes that the scanner will search for in memory.
- header_size - the number of bytes to skip at the start when parsing the class in getClassSize.

Example:
```json
{
    "pattern": "CA FE BA BE 00",
    "header_size": 4
}
```
- The scanner will search memory for all 5 bytes: CA FE BA BE 00.
- When a match is found, getClassSize will skip only the first 4 bytes (CA FE BA BE) before starting to parse the class structure.
- This allows the extra byte(s) at the end of pattern to serve as additional verification or alignment, without affecting the parsing offset.

### Requirements
- Windows
- C++ 17 or newer

### Usage
- Inject the compiled DLL into the target process that contains JVM/class bytes in memory.
- On attach the dumper will start scanning automatically and create .class files in the dum folder.
- Scanning may cause temporary system lag or increased CPU/disk activity - this is normal while the dumper reads process memory.
- After dumping, you can collect all .class files into a single archive and open or decompile them using Recaf or any Java bytecode analysis tool.
Dump results and config located at %appdata%/local/JClassDumper