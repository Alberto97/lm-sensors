/*
    chips_generic.c - Part of sensors, a user-space program for hardware monitoring
    Copyright (c) 1998-2003 Frodo Looijaard <frodol@dds.nl>
                            and Mark D. Studebaker <mdsxyz123@yahoo.com>
    Copyright (c) 2003-2006 The lm_sensors team

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdlib.h>

#include "chips_generic.h"
#include "chips.h"

#define PRINT_SENSORS_LABEL_SIZE 12

static int get_feature_value(const sensors_chip_name *name, 
                             const sensors_feature_data *feature, 
                             double *val)
{
  return sensors_get_feature(*name, feature->number, val);
}

static const sensors_feature_data*
sensors_get_sub_feature_by_type(const sensors_chip_name *name, 
                                const sensors_feature_data *feature, 
                                int i, int j,
                                enum sensors_feature_type type)
{
  const sensors_feature_data *iter;
  
  while((iter = sensors_get_all_features(*name, &i, &j)) && 
      iter->mapping != SENSORS_NO_MAPPING &&
      iter->mapping == feature->number) {
    if (sensors_feature_get_type(iter) == type)
      return iter;
  }
  
  return NULL;
}

static void sensors_get_available_features(const sensors_chip_name *name, 
                                           const sensors_feature_data *feature, 
                                           int i, int j, 
                                           short *has_features, 
                                           double *feature_vals, 
                                           int size, 
                                           int first_val)
{
  const sensors_feature_data *iter;
  
  while((iter = sensors_get_all_features(*name, &i, &j)) && 
      iter->mapping != SENSORS_NO_MAPPING &&
      iter->mapping == feature->number) {
    sensors_feature_type type = sensors_feature_get_type(iter);
    unsigned int index;
    
    if (type == SENSORS_FEATURE_UNKNOWN)
      continue;
    
    index = type - first_val - 1;
    if (index >= size) {
      printf("ERROR: Bug in sensors: index out of bound");
      return;
    }
    
    if (get_feature_value(name, iter, &feature_vals[index]))
      printf("ERROR: Can't get %s data!\n", iter->name);
    
    /* some chips don't have all the features they claim to have */
    /* has_features[index] = (feature_vals[index] != 127); */
    has_features[index] = 1;
  }
}

#define TEMP_FEATURE(x) has_features[x - SENSORS_FEATURE_TEMP - 1]
#define TEMP_FEATURE_VAL(x) feature_vals[x - SENSORS_FEATURE_TEMP - 1]
static void print_generic_chip_temp(const sensors_chip_name *name, 
                                    const sensors_feature_data *feature,
                                    int i, int j)
{
  double val, max, min, lim = 0.0;
  char *label;
  int valid, type;
  const int size = SENSORS_FEATURE_TEMP_CRIT - SENSORS_FEATURE_TEMP;
  short has_features[SENSORS_FEATURE_TEMP_CRIT - SENSORS_FEATURE_TEMP] = {0, };
  double feature_vals[SENSORS_FEATURE_TEMP_CRIT - SENSORS_FEATURE_TEMP] = {0.0, };
  
  if (sensors_get_label_and_valid(*name, feature->number, &label, &valid)) {
    free(label);
    printf("ERROR: Can't get temperature data!\n");
    return;
  } else if (!valid) {
    free(label);
    return; /* ignored */
  }
  
  if (get_feature_value(name, feature, &val)) {
    free(label);
    printf("ERROR: Can't get temperature data!\n");
    return;
  }
  
  sensors_get_available_features(name, feature, i, j, has_features, 
      feature_vals, size, SENSORS_FEATURE_TEMP);
  
  if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MAX)) {
    max = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MAX);
    
    if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MIN)) {
      min = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MIN);
      
      if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_LIM)) {
        lim = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_LIM);
        type = LIM;
      } else {
        type = MINMAX;
      }
    } else if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT)) {
      min = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT);
      type = CRIT;
    } else if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_HYST)) {
      min = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_HYST);
      type = HYST;
    } else {
      min = 0;
      type = MAXONLY;
    }
  } else if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_HYST)) {
    min = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_HYST);
    
    if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_OVER)) {
      max = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_OVER);
      type = HYST;
    } else {
      max = min;
      max = 0;
      type = HYSTONLY;
    }
  } else if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_LOW)) {
    min = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_LOW);
    
    if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_HIGH)) {
      max = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_HIGH);
    } else { /* TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_OVER) */
      max = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_OVER);
    }
    
    type = MINMAX;
  } else if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_OVER)) {
    max = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_OVER);
    min = 0;
    type = MAXONLY;
  } else {
    min = max = 0;
    type = SINGLE;
  }
  
  print_label(label, PRINT_SENSORS_LABEL_SIZE);
  free(label);
  
  if (type == LIM) {
    print_temp_info_real(val, max, min, lim, type, 1, 1);
  } else {
    print_temp_info_real(val, max, min, 0.0, type, 1, 1);
  }
  
  /* TODO: check for FAULT */
  if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_ALARM) && 
      TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_ALARM)) {
    printf(" ALARM");
  }
  printf("\n");
  
  if (type == MINMAX && TEMP_FEATURE(SENSORS_FEATURE_TEMP_CRIT))
  {
    const sensors_feature_data *subfeature;
    max = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_CRIT);
    
    if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_HYST)) {
      min = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_HYST);
      type = HYSTONLY;
    } else if (TEMP_FEATURE(SENSORS_FEATURE_TEMP_MAX) && 
        !TEMP_FEATURE(SENSORS_FEATURE_TEMP_MIN)) {
      min = TEMP_FEATURE_VAL(SENSORS_FEATURE_TEMP_MAX);
      type = HYSTONLY;
    } else {
      type = SINGLE;
      min = 0.0;
    }
    
    subfeature = sensors_get_sub_feature_by_type(name, feature, i, j, 
        SENSORS_FEATURE_TEMP_CRIT);
    if (subfeature) {
      sensors_get_label_and_valid(*name, subfeature->number, &label, &valid);
      
      if (valid) {
        print_label(label, PRINT_SENSORS_LABEL_SIZE);
        free(label);
        print_temp_info_real(max, min, 0, 0.0, type, 1, 0);
        printf("\n");
      }
    }
  }       
}

#define IN_FEATURE(x) has_features[x - SENSORS_FEATURE_IN - 1]
#define IN_FEATURE_VAL(x) feature_vals[x - SENSORS_FEATURE_IN - 1]
static void print_generic_chip_in(const sensors_chip_name *name, 
                                  const sensors_feature_data *feature,
                                  int i, int j)
{
  const int size = SENSORS_FEATURE_IN_MAX_ALARM - SENSORS_FEATURE_IN;
  int valid;
  short has_features[SENSORS_FEATURE_IN_MAX_ALARM - SENSORS_FEATURE_IN] = {0, };
  double feature_vals[SENSORS_FEATURE_IN_MAX_ALARM - SENSORS_FEATURE_IN] = {0.0, };
  double val, alarm_max, alarm_min;
  char *label;
  
  if (sensors_get_label_and_valid(*name, feature->number, &label, &valid)) {
    free(label);
    printf("ERROR: Can't get in data!\n");
    return;
  } else if (!valid) {
    free(label);
    return; /* ignored */
  }
  
  if (get_feature_value(name, feature, &val)) {
    printf("ERROR: Can't get %s data!\n", label);
    return;
  }
  
  sensors_get_available_features(name, feature, i, j, has_features, feature_vals,
      size, SENSORS_FEATURE_IN);
  
  print_label(label, PRINT_SENSORS_LABEL_SIZE);
  free(label);
  printf("%+6.2f V", val);
  
  if (IN_FEATURE(SENSORS_FEATURE_IN_MAX)) {
    printf("  (min = %+6.2f V, max = %+6.2f V)", 
    IN_FEATURE_VAL(SENSORS_FEATURE_IN_MIN),
    IN_FEATURE_VAL(SENSORS_FEATURE_IN_MAX));
    
    if (IN_FEATURE(SENSORS_FEATURE_IN_MAX_ALARM) && 
        IN_FEATURE(SENSORS_FEATURE_IN_MIN_ALARM)) {
      alarm_max = IN_FEATURE_VAL(SENSORS_FEATURE_IN_MAX_ALARM);
      alarm_min = IN_FEATURE_VAL(SENSORS_FEATURE_IN_MIN_ALARM);
      
      if (alarm_min || alarm_max) {
        printf(" ALARM (");
        
        if (alarm_min)
          printf("MIN");
        if (alarm_max)
          printf("%sMAX", (alarm_min) ? "," : "");
        
        printf(")");
      }
    } else if (IN_FEATURE(SENSORS_FEATURE_IN_ALARM)) {
      printf("   %s", 
      IN_FEATURE_VAL(SENSORS_FEATURE_IN_ALARM) > 0.5 ? "ALARM" : "");
    }
  }
  
  printf("\n");
}

#define FAN_FEATURE(x) has_features[x - SENSORS_FEATURE_FAN - 1]
#define FAN_FEATURE_VAL(x) feature_vals[x - SENSORS_FEATURE_FAN - 1]
static void print_generic_chip_fan(const sensors_chip_name *name, 
                                   const sensors_feature_data *feature,
                                   int i, int j)
{
  char *label;
  int valid;
  const int size = SENSORS_FEATURE_FAN_DIV - SENSORS_FEATURE_FAN;
  short has_features[SENSORS_FEATURE_FAN_DIV - SENSORS_FEATURE_FAN] = {0, };
  double feature_vals[SENSORS_FEATURE_FAN_DIV - SENSORS_FEATURE_FAN] = {0.0, };
  double val;
  
  if (sensors_get_label_and_valid(*name, feature->number, &label, &valid)) {
    printf("ERROR: Can't get fan data!\n");
    free(label);
    return;
  } else if (!valid) {
    free(label);
    return; /* ignored */
  }
  
  if (get_feature_value(name, feature, &val))
  {
    printf("ERROR: Can't get %s data!\n", label);
    free(label);
    return;
  }
  
  print_label(label, PRINT_SENSORS_LABEL_SIZE);
  free(label);
  printf("%4.0f RPM", val);
  
  sensors_get_available_features(name, feature, i, j, has_features, feature_vals,
      size, SENSORS_FEATURE_FAN);
  
  if (FAN_FEATURE(SENSORS_FEATURE_FAN_MIN)) {
    printf("  (min = %4.0f RPM", FAN_FEATURE_VAL(SENSORS_FEATURE_FAN_MIN));
    
    if (FAN_FEATURE(SENSORS_FEATURE_FAN_DIV)) {
      printf(", div = %1.0f", FAN_FEATURE_VAL(SENSORS_FEATURE_FAN_DIV));  
    }
    
    printf(")");
  }
  
  if (FAN_FEATURE(SENSORS_FEATURE_FAN_ALARM) && 
      FAN_FEATURE_VAL(SENSORS_FEATURE_FAN_ALARM)) {
    printf(" ALARM");
  }       
  
  printf("\n");
}

static void print_generic_chip_vid(const sensors_chip_name *name, 
                                   const sensors_feature_data *feature,
                                   int i, int j)
{
  const sensors_feature_data *vrm_feature;
  
  while((vrm_feature = sensors_get_all_features(*name, &i, &j))) {
    if (sensors_feature_get_type(vrm_feature) == SENSORS_FEATURE_VRM)
      break;
  }
  
  if (vrm_feature != NULL)
    print_vid_info_real(name, feature->number, vrm_feature->number, PRINT_SENSORS_LABEL_SIZE);
  else
    print_vid_info_real(name, feature->number, -1, PRINT_SENSORS_LABEL_SIZE);
}

void print_generic_chip(const sensors_chip_name *name)
{
  const sensors_feature_data *feature;
  int i,j;
  
  i = j = 0;
  while((feature = sensors_get_all_features(*name, &i, &j))) {
    if (feature->mapping != SENSORS_NO_MAPPING)
      continue;
    
    switch(sensors_feature_get_type(feature)) {
      case SENSORS_FEATURE_TEMP:
        print_generic_chip_temp(name, feature, i, j); break;
      case SENSORS_FEATURE_IN:
        print_generic_chip_in(name, feature, i, j); break;
      case SENSORS_FEATURE_FAN:
        print_generic_chip_fan(name, feature, i, j); break;
      case SENSORS_FEATURE_VID:
        print_generic_chip_vid(name, feature, i, j); break;
      default: continue;
    }
  }
}