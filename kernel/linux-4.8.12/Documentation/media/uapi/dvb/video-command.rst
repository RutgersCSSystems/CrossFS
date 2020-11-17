.. -*- coding: utf-8; mode: rst -*-

.. _VIDEO_COMMAND:

=============
VIDEO_COMMAND
=============

Name
----

VIDEO_COMMAND


Synopsis
--------

.. cpp:function:: int ioctl(int fd, int request = VIDEO_COMMAND, struct video_command *cmd)


Arguments
---------

.. flat-table::
    :header-rows:  0
    :stub-columns: 0


    -  .. row 1

       -  int fd

       -  File descriptor returned by a previous call to open().

    -  .. row 2

       -  int request

       -  Equals VIDEO_COMMAND for this command.

    -  .. row 3

       -  struct video_command \*cmd

       -  Commands the decoder.


Description
-----------

This ioctl is obsolete. Do not use in new drivers. For V4L2 decoders
this ioctl has been replaced by the
:ref:`VIDIOC_DECODER_CMD` ioctl.

This ioctl commands the decoder. The ``video_command`` struct is a
subset of the ``v4l2_decoder_cmd`` struct, so refer to the
:ref:`VIDIOC_DECODER_CMD` documentation for
more information.


Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
