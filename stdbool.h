#ifndef _STDBOOL_H
#define _STDBOOL_H

/* stdbool.h - C99 standard header */

#if __POCC__ >= 500
#pragma once
#endif

/* macros */
#define bool  int
#define false  0
#define true  1
#define __bool_true_false_are_defined  1

#endif /* _STDBOOL_H */