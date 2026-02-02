#include "procedure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

procedure_manager *procedure_manager_create(config_t *config, MetadataManager *metadata) {
    procedure_manager *pm = (procedure_manager *)malloc(sizeof(procedure_manager));
    if (!pm) {
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_PROCEDURE, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for procedure manager");
        return NULL;
    }

    memset(pm, 0, sizeof(procedure_manager));
    pm->config = config;
    pm->metadata = metadata;
    pm->procedures = (stored_procedure *)malloc(MAX_PROCEDURES * sizeof(stored_procedure));
    if (!pm->procedures) {
        free(pm);
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_PROCEDURE, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for procedures");
        return NULL;
    }
    pm->triggers = (trigger *)malloc(MAX_TRIGGERS * sizeof(trigger));
    if (!pm->triggers) {
        free(pm->procedures);
        free(pm);
        error_log(ERROR_LEVEL_ERROR, ERROR_MODULE_PROCEDURE, ERROR_OUT_OF_MEMORY, "Failed to allocate memory for triggers");
        return NULL;
    }

    pm->procedure_count = 0;
    pm->trigger_count = 0;
    pm->initialized = true;

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_PROCEDURE, "Procedure manager created");
    return pm;
}

void procedure_manager_destroy(procedure_manager *pm) {
    if (pm) {
        if (pm->procedures) {
            for (int i = 0; i < pm->procedure_count; i++) {
                stored_procedure *proc = &pm->procedures[i];
                if (proc->name) free(proc->name);
                if (proc->schema) free(proc->schema);
                if (proc->body) free(proc->body);
                if (proc->parameters) {
                    for (int j = 0; j < proc->parameter_count; j++) {
                        procedure_parameter *param = &proc->parameters[j];
                        if (param->name) free(param->name);
                        if (param->default_value) free(param->default_value);
                    }
                    free(proc->parameters);
                }
                if (proc->return_type) free(proc->return_type);
            }
            free(pm->procedures);
        }
        if (pm->triggers) {
            for (int i = 0; i < pm->trigger_count; i++) {
                trigger *trig = &pm->triggers[i];
                if (trig->name) free(trig->name);
                if (trig->schema) free(trig->schema);
                if (trig->table_name) free(trig->table_name);
                if (trig->body) free(trig->body);
            }
            free(pm->triggers);
        }
        free(pm);
    }
}

int procedure_create(procedure_manager *pm, const char *name, const char *schema, int type, const char *body, procedure_parameter *parameters, int parameter_count, const char *return_type, bool deterministic, bool sql_security_definer) {
    if (!pm || !name || !body) {
        return ERROR_INVALID_PARAMETER;
    }

    if (pm->procedure_count >= MAX_PROCEDURES) {
        return ERROR_LIMIT_EXCEEDED;
    }

    for (int i = 0; i < pm->procedure_count; i++) {
        stored_procedure *proc = &pm->procedures[i];
        if (strcmp(proc->name, name) == 0 && (!schema || !proc->schema || strcmp(proc->schema, schema) == 0)) {
            return ERROR_ALREADY_EXISTS;
        }
    }

    stored_procedure *proc = &pm->procedures[pm->procedure_count++];
    proc->name = strdup(name);
    if (schema) {
        proc->schema = strdup(schema);
    }
    proc->type = type;
    proc->body = strdup(body);
    if (parameters && parameter_count > 0) {
        proc->parameters = (procedure_parameter *)malloc(parameter_count * sizeof(procedure_parameter));
        if (proc->parameters) {
            for (int i = 0; i < parameter_count; i++) {
                procedure_parameter *param = &proc->parameters[i];
                procedure_parameter *src_param = &parameters[i];
                param->name = strdup(src_param->name);
                param->type = src_param->type;
                param->data_type = src_param->data_type;
                param->data_length = src_param->data_length;
                if (src_param->default_value) {
                    param->default_value = strdup(src_param->default_value);
                }
            }
            proc->parameter_count = parameter_count;
        }
    }
    if (return_type) {
        proc->return_type = strdup(return_type);
    }
    proc->deterministic = deterministic;
    proc->sql_security_definer = sql_security_definer;
    proc->created_at = (uint64_t)time(NULL);
    proc->modified_at = proc->created_at;

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_PROCEDURE, "Created procedure %s", name);
    return SUCCESS;
}

int procedure_drop(procedure_manager *pm, const char *name, const char *schema) {
    if (!pm || !name) {
        return ERROR_INVALID_PARAMETER;
    }

    for (int i = 0; i < pm->procedure_count; i++) {
        stored_procedure *proc = &pm->procedures[i];
        if (strcmp(proc->name, name) == 0 && (!schema || !proc->schema || strcmp(proc->schema, schema) == 0)) {
            if (proc->name) free(proc->name);
            if (proc->schema) free(proc->schema);
            if (proc->body) free(proc->body);
            if (proc->parameters) {
                for (int j = 0; j < proc->parameter_count; j++) {
                    procedure_parameter *param = &proc->parameters[j];
                    if (param->name) free(param->name);
                    if (param->default_value) free(param->default_value);
                }
                free(proc->parameters);
            }
            if (proc->return_type) free(proc->return_type);
            memmove(&pm->procedures[i], &pm->procedures[i + 1], (pm->procedure_count - i - 1) * sizeof(stored_procedure));
            pm->procedure_count--;
            logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_PROCEDURE, "Dropped procedure %s", name);
            return SUCCESS;
        }
    }

    return ERROR_NOT_FOUND;
}

int procedure_execute(procedure_manager *pm, const char *name, const char *schema, void **parameters, void **result) {
    if (!pm || !name) {
        return ERROR_INVALID_PARAMETER;
    }

    stored_procedure *proc = procedure_get(pm, name, schema);
    if (!proc) {
        return ERROR_NOT_FOUND;
    }

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_PROCEDURE, "Executing procedure %s", name);

    return SUCCESS;
}

stored_procedure *procedure_get(procedure_manager *pm, const char *name, const char *schema) {
    if (!pm || !name) {
        return NULL;
    }

    for (int i = 0; i < pm->procedure_count; i++) {
        stored_procedure *proc = &pm->procedures[i];
        if (strcmp(proc->name, name) == 0 && (!schema || !proc->schema || strcmp(proc->schema, schema) == 0)) {
            return proc;
        }
    }

    return NULL;
}

int trigger_create(procedure_manager *pm, const char *name, const char *schema, const char *table_name, int events, int timing, const char *body, bool enabled) {
    if (!pm || !name || !table_name || !body) {
        return ERROR_INVALID_PARAMETER;
    }

    if (pm->trigger_count >= MAX_TRIGGERS) {
        return ERROR_LIMIT_EXCEEDED;
    }

    for (int i = 0; i < pm->trigger_count; i++) {
        trigger *trig = &pm->triggers[i];
        if (strcmp(trig->name, name) == 0 && (!schema || !trig->schema || strcmp(trig->schema, schema) == 0)) {
            return ERROR_ALREADY_EXISTS;
        }
    }

    trigger *trig = &pm->triggers[pm->trigger_count++];
    trig->name = strdup(name);
    if (schema) {
        trig->schema = strdup(schema);
    }
    trig->table_name = strdup(table_name);
    trig->events = events;
    trig->timing = timing;
    trig->body = strdup(body);
    trig->enabled = enabled;
    trig->created_at = (uint64_t)time(NULL);
    trig->modified_at = trig->created_at;

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_PROCEDURE, "Created trigger %s on table %s", name, table_name);
    return SUCCESS;
}

int trigger_drop(procedure_manager *pm, const char *name, const char *schema) {
    if (!pm || !name) {
        return ERROR_INVALID_PARAMETER;
    }

    for (int i = 0; i < pm->trigger_count; i++) {
        trigger *trig = &pm->triggers[i];
        if (strcmp(trig->name, name) == 0 && (!schema || !trig->schema || strcmp(trig->schema, schema) == 0)) {
            if (trig->name) free(trig->name);
            if (trig->schema) free(trig->schema);
            if (trig->table_name) free(trig->table_name);
            if (trig->body) free(trig->body);
            memmove(&pm->triggers[i], &pm->triggers[i + 1], (pm->trigger_count - i - 1) * sizeof(trigger));
            pm->trigger_count--;
            logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_PROCEDURE, "Dropped trigger %s", name);
            return SUCCESS;
        }
    }

    return ERROR_NOT_FOUND;
}

int trigger_enable(procedure_manager *pm, const char *name, const char *schema, bool enable) {
    if (!pm || !name) {
        return ERROR_INVALID_PARAMETER;
    }

    trigger *trig = trigger_get(pm, name, schema);
    if (!trig) {
        return ERROR_NOT_FOUND;
    }

    trig->enabled = enable;
    trig->modified_at = (uint64_t)time(NULL);

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_PROCEDURE, "%s trigger %s", enable ? "Enabled" : "Disabled", name);
    return SUCCESS;
}

int trigger_fire(procedure_manager *pm, const char *table_name, int event, int timing, void *context) {
    if (!pm || !table_name) {
        return ERROR_INVALID_PARAMETER;
    }

    for (int i = 0; i < pm->trigger_count; i++) {
        trigger *trig = &pm->triggers[i];
        if (strcmp(trig->table_name, table_name) == 0 && trig->enabled && (trig->events & event) && (trig->timing & timing)) {
            logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_PROCEDURE, "Firing trigger %s on table %s for event %d at timing %d", trig->name, table_name, event, timing);
        }
    }

    return SUCCESS;
}

trigger *trigger_get(procedure_manager *pm, const char *name, const char *schema) {
    if (!pm || !name) {
        return NULL;
    }

    for (int i = 0; i < pm->trigger_count; i++) {
        trigger *trig = &pm->triggers[i];
        if (strcmp(trig->name, name) == 0 && (!schema || !trig->schema || strcmp(trig->schema, schema) == 0)) {
            return trig;
        }
    }

    return NULL;
}

int procedure_manager_load_from_metadata(procedure_manager *pm) {
    if (!pm || !pm->metadata) {
        return ERROR_INVALID_PARAMETER;
    }

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_PROCEDURE, "Loading procedures and triggers from metadata");

    return SUCCESS;
}

int procedure_manager_save_to_metadata(procedure_manager *pm) {
    if (!pm || !pm->metadata) {
        return ERROR_INVALID_PARAMETER;
    }

    logging_log(LOG_LEVEL_INFO, LOGGING_MODULE_PROCEDURE, "Saving procedures and triggers to metadata");

    return SUCCESS;
}

void procedure_print(stored_procedure *proc) {
    if (!proc) {
        return;
    }

    printf("Procedure: %s\n", proc->name);
    if (proc->schema) {
        printf("Schema: %s\n", proc->schema);
    }
    printf("Type: %s\n", proc->type == PROCEDURE_TYPE_STORED ? "Stored Procedure" : "Function");
    printf("Body: %s\n", proc->body);
    if (proc->parameter_count > 0) {
        printf("Parameters:\n");
        for (int i = 0; i < proc->parameter_count; i++) {
            procedure_parameter *param = &proc->parameters[i];
            printf("  %s (%s, %s)\n", param->name, 
                   param->type == PARAMETER_TYPE_IN ? "IN" : param->type == PARAMETER_TYPE_OUT ? "OUT" : "INOUT",
                   param->data_type == DATA_TYPE_INT ? "INT" : param->data_type == DATA_TYPE_VARCHAR ? "VARCHAR" : "UNKNOWN");
        }
    }
    if (proc->return_type) {
        printf("Return Type: %s\n", proc->return_type);
    }
    printf("Deterministic: %s\n", proc->deterministic ? "Yes" : "No");
    printf("SQL Security Definer: %s\n", proc->sql_security_definer ? "Yes" : "No");
    printf("Created: %llu\n", (unsigned long long)proc->created_at);
    printf("Modified: %llu\n", (unsigned long long)proc->modified_at);
}

void trigger_print(trigger *trig) {
    if (!trig) {
        return;
    }

    printf("Trigger: %s\n", trig->name);
    if (trig->schema) {
        printf("Schema: %s\n", trig->schema);
    }
    printf("Table: %s\n", trig->table_name);
    printf("Events: %s%s%s\n", 
           trig->events & TRIGGER_EVENT_INSERT ? "INSERT " : "",
           trig->events & TRIGGER_EVENT_UPDATE ? "UPDATE " : "",
           trig->events & TRIGGER_EVENT_DELETE ? "DELETE" : "");
    printf("Timing: %s%s%s\n", 
           trig->timing & TRIGGER_TIMING_BEFORE ? "BEFORE " : "",
           trig->timing & TRIGGER_TIMING_AFTER ? "AFTER " : "",
           trig->timing & TRIGGER_TIMING_INSTEAD_OF ? "INSTEAD OF" : "");
    printf("Body: %s\n", trig->body);
    printf("Enabled: %s\n", trig->enabled ? "Yes" : "No");
    printf("Created: %llu\n", (unsigned long long)trig->created_at);
    printf("Modified: %llu\n", (unsigned long long)trig->modified_at);
}
