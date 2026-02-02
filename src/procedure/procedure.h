#ifndef PROCEDURE_H
#define PROCEDURE_H

#include "../config/config.h"
#include "../error/error.h"
#include "../logging/logging.h"
#include "../metadata/metadata.h"

#define PROCEDURE_MODULE "procedure"

#define MAX_PROCEDURE_NAME_LENGTH 256
#define MAX_PROCEDURE_BODY_LENGTH 10240
#define MAX_PARAMETERS_PER_PROCEDURE 64
#define MAX_TRIGGER_NAME_LENGTH 256
#define MAX_TRIGGER_BODY_LENGTH 10240
#define MAX_PROCEDURES 1024
#define MAX_TRIGGERS 1024

#define PROCEDURE_TYPE_STORED 1
#define PROCEDURE_TYPE_FUNCTION 2

#define TRIGGER_EVENT_INSERT 1
#define TRIGGER_EVENT_UPDATE 2
#define TRIGGER_EVENT_DELETE 4
#define TRIGGER_EVENT_ALL (TRIGGER_EVENT_INSERT | TRIGGER_EVENT_UPDATE | TRIGGER_EVENT_DELETE)

#define TRIGGER_TIMING_BEFORE 1
#define TRIGGER_TIMING_AFTER 2
#define TRIGGER_TIMING_INSTEAD_OF 4

#define PARAMETER_TYPE_IN 1
#define PARAMETER_TYPE_OUT 2
#define PARAMETER_TYPE_INOUT 3

#define DATA_TYPE_INT 1
#define DATA_TYPE_VARCHAR 2
#define DATA_TYPE_FLOAT 3
#define DATA_TYPE_DOUBLE 4
#define DATA_TYPE_BOOL 5
#define DATA_TYPE_DATE 6
#define DATA_TYPE_TIME 7
#define DATA_TYPE_DATETIME 8
#define DATA_TYPE_BLOB 9

// 参数结构
typedef struct {
    char *name;
    int type;
    int data_type;
    int data_length;
    char *default_value;
} procedure_parameter;

// 存储过程结构
typedef struct {
    char *name;
    char *schema;
    int type;
    char *body;
    procedure_parameter *parameters;
    int parameter_count;
    char *return_type;
    bool deterministic;
    bool sql_security_definer;
    uint64_t created_at;
    uint64_t modified_at;
} stored_procedure;

// 触发器结构
typedef struct {
    char *name;
    char *schema;
    char *table_name;
    int events;
    int timing;
    char *body;
    bool enabled;
    uint64_t created_at;
    uint64_t modified_at;
} trigger;

// 存储过程和触发器管理器结构
typedef struct {
    config_t *config;
    MetadataManager *metadata;
    stored_procedure *procedures;
    int procedure_count;
    trigger *triggers;
    int trigger_count;
    bool initialized;
} procedure_manager;

procedure_manager *procedure_manager_create(config_t *config, MetadataManager *metadata);
void procedure_manager_destroy(procedure_manager *pm);

int procedure_create(procedure_manager *pm, const char *name, const char *schema, int type, const char *body, procedure_parameter *parameters, int parameter_count, const char *return_type, bool deterministic, bool sql_security_definer);
int procedure_drop(procedure_manager *pm, const char *name, const char *schema);
int procedure_execute(procedure_manager *pm, const char *name, const char *schema, void **parameters, void **result);
stored_procedure *procedure_get(procedure_manager *pm, const char *name, const char *schema);

int trigger_create(procedure_manager *pm, const char *name, const char *schema, const char *table_name, int events, int timing, const char *body, bool enabled);
int trigger_drop(procedure_manager *pm, const char *name, const char *schema);
int trigger_enable(procedure_manager *pm, const char *name, const char *schema, bool enable);
int trigger_fire(procedure_manager *pm, const char *table_name, int event, int timing, void *context);
trigger *trigger_get(procedure_manager *pm, const char *name, const char *schema);

int procedure_manager_load_from_metadata(procedure_manager *pm);
int procedure_manager_save_to_metadata(procedure_manager *pm);

void procedure_print(stored_procedure *proc);
void trigger_print(trigger *trig);

#endif