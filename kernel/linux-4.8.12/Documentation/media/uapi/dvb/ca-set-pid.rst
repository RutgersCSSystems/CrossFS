.. -*- coding: utf-8; mode: rst -*-

.. _CA_SET_PID:

==========
CA_SET_PID
==========

Name
----

CA_SET_PID


Synopsis
--------

.. cpp:function:: int  ioctl(fd, int request = CA_SET_PID, ca_pid_t *)


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

       -  Equals CA_SET_PID for this command.

    -  .. row 3

       -  ca_pid_t \*

       -  Undocumented.


Description
-----------

This ioctl is undocumented. Documentation is welcome.


Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
