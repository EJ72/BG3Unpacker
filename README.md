Introduction: 
The BG3Unpacker is a command-line tool designed specifically for extracting files from Larian Studios' Baldur's Gate 3 (Release version ONLY). This tool simplifies the process of unpacking and accessing the contents of Larian's archive formats, offering support for ZLIB, LZ4, and ZSTD.
Usage: To utilize the BG3Unpacker, follow these straightforward steps:
1.	Download the Tool: Ensure you have the BG3Unpacker executable. Obtain the tool from a trusted source or compile it from the provided source code. (To be Determined)
   
3.	Command-Line Syntax: Open a command prompt or terminal window and navigate to the directory containing the BG3Unpacker executable. The basic syntax for running the tool is as follows:
   
BG3Unpacker <input_file>

Drag and Drop:
An alternative way to use BG3Unpacker is by dragging and dropping the game archive directly onto the executable file. Simply locate the BG3Unpacker executable, drag the target game archive onto the executable, and the tool will automatically begin the extraction process.
Replace <input_file> with the path to the BG3 game archive you want to unpack.
3.	Supported File Types: BG3Unpacker supports game archives with the extensions “. pak" and ".lsv." Ensure that your input file has a valid extension for successful processing.

4.	Output Directory: The extracted files will be placed in a directory named after the input file's stem (excluding the extension). The tool automatically creates this directory in the same location as the input file.
   
6.	Monitoring Progress: During the extraction process, the tool provides information about the decompression of each file, including the compressed size, expected decompressed size, and progress percentage.
   
8.	Completion Time: After the extraction is complete, the tool displays the total time elapsed for the entire process.
   

Example Usage:
BG3Unpacker Assets.pak 
This command will extract the contents of " Assets.pak " into a directory named " Assets " in the same location as the input file.

TODO:
1.	Support multi-archives for Textures and VirtualTextures (Only the contents of the first archive extract)
   
3.	Figure out why Gustav.pak, Models.pak, and SharedSounds.pak do not extract fully.

Source code only, it needs work definitely.



All files extracted from this software are the exclusive property of Larian Studios.
By accessing or extracting these files, you acknowledge and agree to Larian Studios' ownership and proprietary rights.
