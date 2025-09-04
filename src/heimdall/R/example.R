# Rscript -e "devtools::build(binary = TRUE, args = c())"
# R CMD INSTALL ../<file>.tar.gz

library(tibble)
library(heimdallR)

hm <- hm_open('../../../tmp/heimdall/projects/test.db')

hm_set_domain(hm, 'Test', tribble(
    ~name, ~path,
    'AA', '/Niveau0/AA',
    'B', '/Niveau0/B',
    'C', '/Niveau0/C',
    'D', '/Niveau1/sub/D',
    'E', '/Niveau1/sub/E'
))

hm_set_view(hm, 'Test view', tribble(
    ~path, ~domain, ~concept,
    '/AA', 'Test', 'AA',
    '/B', 'Test', 'B'
))

hm_add_events(hm, tribble(
    ~entity, ~domain, ~concept, ~time, ~warning,
    'ENT1', 'Test', 'AA', 1, FALSE,
    'ENT1', 'Test', 'AA', 3, TRUE,
    'ENT1', 'Test', 'B', 2, FALSE,
    'ENT2', 'Test', 'AA', 1, FALSE,
    'ENT2', 'Test', 'D', 5, FALSE,
    'ENT2', 'Test', 'E', 6, FALSE,
    'ENT2', 'Test', 'E', 7, FALSE,
    'ENT2', 'Test', 'E', 9, TRUE
))

hm_add_periods(hm, tribble(
    ~entity, ~domain, ~concept, ~time, ~duration,
    'ENT1', 'Test', 'D', 4, 20,
    'ENT1', 'Test', 'D', 6, 4
))

hm_add_values(hm, tribble(
    ~entity, ~domain, ~concept, ~time, ~value, ~warning,
    'ENT1', 'Test', 'C', 4, 20, TRUE,
    'ENT1', 'Test', 'C', 6, 4, FALSE,
    'ENT1', 'Test', 'C', 8, 4, FALSE,
    'ENT1', 'Test', 'C', 12, 5, FALSE
))
