# NAME

nmbe - Native Messaging Browser Extension helper tool.

# SYNOPSIS

```
nmbe [-v] [-h] [-m msg] [-f file] [-b base64]
```


# DESCRIPTION

The tool allows the passing of one or more messages to a native messag-
ing browser extension for  the  purposes  of  development,  testing  or
debugging.

Each message is structured as a platform native 32 bit unsigned integer
containing the length of the message, followed by the  message  itself.
These  messages  are  written to stdout in the expectation that they be
piped to the native messaging browser extension under test.

Messages can be specified as parameters on the command line, or by ref-
erence to a file or directory. Stdin can be specified with '-'.

# OPTIONS

       -m, --message msg
              String to send as a message.

       -f, --message-file file
              Name of file or directory to send as messages. '-' for stdin.

       -b, --message-base64 b64
              Base64 encoded array of bytes to send as a message.

       -h, --help
              Display this help message.

       -v, --version
              Display the version number.

# RETURN VALUE

The  nmbe  tool  returns  a non zero exit code if the tool is unable to
read any of the messages passed, or if output cannot be written to std-
out.

# EXAMPLES

In  this  example,  we send three separate messages, the first a simple
string, the second a base64 byte array, and the third a file  referring
to stdin.

```
~$ echo "{command:'baz'}" | nmbe --message "{command:'foo'}"          --message-base64 "e2NvbW1hbmQ6J2Jhcid9" --message-file -
```

This results in the following three messages being sent:

```
{command:'foo'}
{command:'bar'}
{command:'baz'}
```


