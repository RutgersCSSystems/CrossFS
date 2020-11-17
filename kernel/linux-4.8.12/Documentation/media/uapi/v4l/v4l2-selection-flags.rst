.. -*- coding: utf-8; mode: rst -*-

.. _v4l2-selection-flags:

***************
Selection flags
***************


.. _v4l2-selection-flags-table:

.. flat-table:: Selection flag definitions
    :header-rows:  1
    :stub-columns: 0


    -  .. row 1

       -  Flag name

       -  id

       -  Definition

       -  Valid for V4L2

       -  Valid for V4L2 subdev

    -  .. row 2

       -  ``V4L2_SEL_FLAG_GE``

       -  (1 << 0)

       -  Suggest the driver it should choose greater or equal rectangle (in
	  size) than was requested. Albeit the driver may choose a lesser
	  size, it will only do so due to hardware limitations. Without this
	  flag (and ``V4L2_SEL_FLAG_LE``) the behaviour is to choose the
	  closest possible rectangle.

       -  Yes

       -  Yes

    -  .. row 3

       -  ``V4L2_SEL_FLAG_LE``

       -  (1 << 1)

       -  Suggest the driver it should choose lesser or equal rectangle (in
	  size) than was requested. Albeit the driver may choose a greater
	  size, it will only do so due to hardware limitations.

       -  Yes

       -  Yes

    -  .. row 4

       -  ``V4L2_SEL_FLAG_KEEP_CONFIG``

       -  (1 << 2)

       -  The configuration must not be propagated to any further processing
	  steps. If this flag is not given, the configuration is propagated
	  inside the subdevice to all further processing steps.

       -  No

       -  Yes
