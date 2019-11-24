# BURO - freeware command line incremental backup utility for MS Windows.<p> #
<p align="center"><img src="logo_buro.png"/></p>
BURO supports data compression and encryption. You can access all your files and it's previous versions with your favorite file manager.<p>
 
[Click here to download](https://github.com/sapozhnikov/keeper/releases "Downloads page")
>I'll be very appreciated if someone will help me to correct this translation. Dmitriy.
## How to backup ##
Simple example how to backup single folder:<p>
>buro --backup --srcdir="<source directory\>" --dstdir="<destination directory\>"<p>

If *source directory* contains files:
>file1<br>
>file2<br>
>...<br>
>fileN<p>

new repository will be created at the *destination directory*:
><mirror\><br>
>>file1<br>
>>file2<br>
>>...<br>
>>fileN<br>

>buro.db<p>

If you change *file1* and run the same command again, *file1* will be moved to the folder, named as current time, and changed file will be copied to *mirror* folder. Example:
><2017-08-31T00-37-12.048\><p>
>>file1 <-old file<p>
>
><mirror\><br>
>>file1 <- newer file<br>
>>file2<br>
>>...<br>
>>fileN<br>

>buro.db<p>
 
If you want to enable compression, add argument "--compress". You can set compression ratio from 1 (faster) to 9 (slower):<p>
>buro --backup --srcdir="<source directory\>" --dstdir="<destination directory\>" --compress=9<p>

Extension ".bz2" will be added to the names at *destination directory*:<p>
>file1.bz2<br>
>file2.bz2<br>
>...<br>
>fileN.bz2<p>

If you want to protect your files with password, add argument *--password*:<p>
>buro --backup --srcdir="<source directory\>" --dstdir="<destination directory\>" --password="<your password\>"<p>

Extension ".encrypted" will be added to the names at *destination directory* and files will be encrypted with the strong encryption algorithm:<p>
>file1.encrypted<br>
>file2.encrypted<br>
>...<br>
>fileN.encrypted<p>

With argument *--encryptnames* file names will encrypted and looks like a garbage:<br>
>DXJKXWIXMQ<br>
>DXJKXWIXQY<br>
>...<br>
>DXJKXWLFKE<br>

You can combine *--compress* and *--password* arguments.<p>
## How to restore ##
 
Restoring is simple:<p>
>buro --restore --srcdir="<source directory\>" --dstdir="<destination directory\>"<p>

Argument *srcdir* is pointing to the repository folder. Latest state of the files will be restored. If you want to restore files state at the given time, define it with argument *--timestamp*. Time's format is "YYYY-MM-DD
HH:mm:SS.SSS".<p>
Do not forget to define password with argument *--password*, if backup was protected with password, or restoring will fail.<p>
## How to delete old diffs ##
You can delete old diffs to save disk space:<p>
>buro --purge --srcdir="<source directory\>" --older=<period or date\><p>

Argument *srcdir* is pointing to the repository folder. You can define the date (format is "YYYY-MM-DD HH:mm:SS.SSS") or period (format is "PxYxMxDTxHxMxS"). For example, older than 1 month is "P1M", older than 1 minute is "PT1M", older than 1 year is "P1Y", one month and 5 days is "P1M5D".<p>

### Filtering ###
It is possible to exclude some directories or files while backuping or restoring. Wildmasks "*" and "?" are supported. You can define more than one mask, just separate them with symbol ";". If argument *--include* is defined, only matched files will be proceeded. If argument *--exclude* is defined, matched files will be skipped. Examples:
>buro --backup --srcdir="<source directory\>" --dstdir="<destination directory\>" --exclude="\*.tmp;\*.bak"<p>
>buro --restore --srcdir="<source directory\>" --dstdir="<destination directory\>" --include="documents\*"<p>

<br/><br/>
