#!/usr/bin/env Rscript

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

local({
    prev_warn <- getOption("warn")
    options(warn = -1)

    library(data.table, warn.conflicts = FALSE)
    library(stringr, warn.conflicts = FALSE)
    library(zoo, warn.conflicts = FALSE)
    library(tibble, warn.conflicts = FALSE)
    library(optparse, warn.conflicts = FALSE)

    options(warn = prev_warn)
})

# Include variables from these varsets
# rdv, consultant are implicit, don't try to put them in
VARSETS <- c(
    'aq1',
    'aq2',
    'neuropsy',
    'ems',
    'diet',
    'demo',
    'constantes',
    'vision',
    'audition',
    'respi'
)

VARIABLES_CALCULEES <- tribble(
    ~varset, ~field_name,
    'consultant', 'age'
)

read_variables <- function(filename, select, encoding = 'UTF-8') {
    variables <- fread(filename, encoding = encoding, na.strings = '')
    variables$varset <- na.locf(variables$varset)
    variables <- variables[type == 'V',]

    variables <- rbind(
        variables,
        VARIABLES_CALCULEES,
        fill = TRUE
    )
    variables <- variables[varset %in% c('rdv', 'consultant', select),]

    # FIXME: Dataquery and checkbox values don't mix well (queries get very slow)
    # variables <- variables[is.na(field_type) | field_type != 'checkbox',]
    # FIXME: Date variables break the Voozanoo CSV export (needs to be investigated)
    variables <- variables[field_name != 'date_creation',]

    return (variables)
}

read_dictionaries <- function(filename, select, encoding = 'UTF-8') {
    dictionaries <- fread(filename, encoding = encoding)
    dictionaries <- dictionaries[dico_name %in% select,]

    return (dictionaries)
}

output_bridge <- function(variables, dictionaries) {
    cat('    const DictInfo = {\n')
    for (name in unique(dictionaries$dico_name)) {
        dict <- dictionaries[dictionaries$dico_name == name,]

        cat(str_interp('        ${name}: {'))
        for (i in 1:nrow(dict)) {
            escaped_code <- gsub('\'', '\\\'', dict[i,]$code, fixed = TRUE)
            escaped_label <- gsub('\'', '\\\'', dict[i,]$label, fixed = TRUE)

            if (grepl('^[0-9]+$', escaped_code)) {
                cat(str_interp(' ${escaped_code}: \'${escaped_label}\','))
            } else {
                cat(str_interp(' \'${escaped_code}\': \'${escaped_label}\','))
            }
        }
        cat(str_interp(' },\n'))
    }
    cat('    };\n\n')

    cat('    const VarInfo = {\n')
    for (i in 1:nrow(variables)) {
        var <- variables[i,]

        csv_name <- paste(var$varset, var$field_name, sep = '.')
        js_name <- paste(var$varset, sub('.', '_', var$field_name, fixed = TRUE), sep = '_')

        if (var$field_type %in% c('text', 'text_multiline', 'date')) {
            cat(str_interp('        ${js_name}: { kind: \'text\' },\n'))
        } else if (var$field_type %in% c('radio', 'list')) {
            cat(str_interp('        ${js_name}: { kind: \'enum\', dict_name: \'${var$dico}\' },\n'))
        } else if (var$field_type %in% 'checkbox') {
            cat(str_interp('        ${js_name}: { kind: \'multi\', dict_name: \'${var$dico}\' },\n'))
        } else if (var$field_type %in% 'integer') {
            cat(str_interp('        ${js_name}: { kind: \'integer\' },\n'))
        } else if (var$field_type %in% 'decimal') {
            cat(str_interp('        ${js_name}: { kind: \'float\' },\n'))
        }
    }
    cat('    };\n')
}

output_dataquery <- function(variables, varsets, condition) {
    cat('<?xml version="1.0"?>
<dataquery id="pl_premier_rdv" table_name="{pj}_rdv_data" varset_name="rdv" table_alias="rdv">')

    # Fields
    for (i in 1:nrow(variables)) {
        var <- variables[i,]
        cat(str_interp('
    <column sql="{var}" alias="${var$varset}.${var$field_name}" default_short_label="${var$varset}.${var$field_name}">
        <field table_name="${var$varset}" field_name="${var$field_name}" alias="var"/>
    </column>'))
    }
    cat('\n')

    # Joins
    for (v in varsets) {
        cat(str_interp('
    <column sql="{var}" alias="${v}.id_data" default_short_label="${v}.id_data" type="integer">
        <field table_name="${v}" field_name="id_data" alias="var"/>
    </column>
    <join detail_table="{pj}_${v}_data" detail_alias="${v}" detail_varset_name="${v}" sql="{from} = {to}" type="left">
        <field table_name="${v}" field_name="id_rdv" alias="from"/>
        <field table_name="rdv" field_name="id_data" alias="to"/>
    </join>'))
    }
    cat('\n')

    cat('
    <!-- RDV et consultants -->
    <column sql="{var}" alias="rdv.id_data" default_short_label="rdv.id_data" type="integer">
        <field table_name="rdv" field_name="id_data" alias="var"/>
    </column>
    <column sql="{var}" alias="consultant.id_data" default_short_label="consultant.id_data" type="integer">
        <field table_name="consultant" field_name="id_data" alias="var"/>
    </column>
    <join detail_table="{pj}_consultant_data" detail_alias="consultant" detail_varset_name="consultant" sql="{from} = {to}" type="left">
        <field table_name="rdv" field_name="id_consultant" alias="from"/>
        <field table_name="consultant" field_name="id_data" alias="to"/>
    </join>')
    cat('\n')

    if (condition == 'first_pl') {
        cat('
    <!-- Condition sur l\'activité PL -->
    <join detail_table="{pj}_creneau_data" detail_alias="creneau" sql="{rdv.id_creneau} = {creneau.id_data}">
        <field table_name="creneau" field_name="id_data" alias="creneau.id_data"/>
        <field table_name="rdv" field_name="id_creneau" alias="rdv.id_creneau"/>
    </join>
    <join detail_table="{pj}_demijournee_data" detail_alias="demijournee" sql="{creneau.id_demijournee} = {demijournee.id_data}">
        <field table_name="demijournee" field_name="id_data" alias="demijournee.id_data"/>
        <field table_name="creneau" field_name="id_demijournee" alias="creneau.id_demijournee"/>
    </join>
    <join detail_table="{pj}_dico_data" detail_alias="activite" sql="{d.id_data} = {demijournee.activite} AND {d.dico_name} = \'activite\'">
        <field table_name="demijournee" field_name="activite" alias="demijournee.activite"/>
        <field table_name="activite" field_name="id_data" alias="d.id_data"/>
        <field table_name="activite" field_name="dico_name" alias="d.dico_name"/>
    </join>
    <condition sql="{activite} = 2" optional="false">
        <field table_name="activite" field_name="code" alias="activite"/>
    </condition>')
    } else {
        stop(str_interp('Unknown condition ${condition}'))
    }

    cat('
</dataquery>')
}

opt <- parse_args(
    OptionParser(
        usage = '%prog [options] pages_file dico_file',
        option_list = list(
            make_option(c('-m', '--mode'), action = "store", default = 'bridge',
                        help = 'Output type: bridge (default), dataquery_first_pl')
        )
    ),
    positional_arguments = 2
)
pages_filename <- opt$args[1]
dictionaries_filename <- opt$args[2]

variables <- read_variables(pages_filename, VARSETS)
dictionaries <- read_dictionaries(dictionaries_filename, na.omit(variables$dico))

if (opt$options$mode == 'bridge') {
    output_bridge(variables, dictionaries)
} else if (opt$options$mode == 'dataquery_first_pl') {
    output_dataquery(variables, VARSETS, condition = 'first_pl')
} else {
    stop(str_interp('Unknown mode ${opt$options$mode}'))
}
