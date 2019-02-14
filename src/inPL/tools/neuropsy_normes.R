library(data.table)
library(stringr)

dt <- fread('neuropsy_normes.csv', encoding = 'UTF-8')

dt2 <- data.table(
    keyp = dt$concat,
    moca_mean = dt$`moy MoCA`,
    moca_sd = dt$`ET MoCA`,
    moca_c50 = dt$`C50 MoCA`,
    moca_c5 = dt$`C5 MoCA`,
    rl_mean = dt$`moy 3RL`,
    rl_sd = dt$`ET 3RL`,
    rl_c50 = dt$`C50 RL`,
    rl_c5 = dt$`C5 3RL`,
    rt_mean = dt$`moy 3RT`,
    rt_sd = dt$`ET 3RT`,
    rt_c50 = dt$`C50 3RT`,
    rt_c5 = dt$`C5 3RT`,
    p_mean = dt$`Fluences (P)`,
    p_sd = dt$`ET (P)`,
    p_c50 = dt$`C50 Fluences (P)`,
    p_c5 = dt$`C50 Fluences (P)`,
    animx_mean = dt$`Fluences ANIMX`,
    animx_sd = dt$`ET (ANIMX)`,
    animx_c50 = dt$`C50 Animx`,
    animx_c5 = dt$`C5 ANIMX`,
    tmta_mean = dt$`TMT A (tps moy)`,
    tmta_sd = dt$`TMTA (Tps ET)`,
    tmta_c50 = dt$`TMT A C50`,
    tmta_c5 = dt$`TMT A C5`,
    tmtb_mean = dt$`TMT B (tps moy)`,
    tmtb_sd = dt$`TMTB(Tps ET)`,
    tmtb_c50 = dt$`TMT B C50`,
    tmtb_c5 = dt$`TMTB C5`,
    deno_mean = dt$`STROOP m(déno)`,
    deno_sd = dt$`STROOP ET(Déno)`,
    deno_c50 = dt$`Déno C50`,
    deno_c5 = dt$`Déno C5`,
    lecture_mean = dt$`Stroop m(Lect)`,
    lecture_sd = dt$`ET (lecture)`,
    lecture_c50 = dt$`lecture C50`,
    lecture_c5 = dt$`Lecture C5`,
    interf_mean = dt$`m (interférence)`,
    interf_sd = dt$`ET (interférence)`,
    interf_c50 = dt$`C50 interférence`,
    interf_c5 = dt$`C5 interférence`,
    int_deno_mean = dt$`m int-déno`,
    int_deno_sd = dt$`ET int-déno`,
    int_deno_c50 = dt$`int-Déno C50`,
    int_deno_c5 = dt$`int-Déno C5`,
    code_c50 = dt$`code C50`,
    code_c5 = dt$`code C5`,
    slc_c50 = dt$`SLC C50`,
    slc_c5 = dt$`SLC C5`
)
setnames(dt2, 'keyp', 'key')

capture.output({
    for (i in 1:nrow(dt2)) {
        row <- dt2[i,]

        keys <- colnames(dt2)[-1]
        dict <- paste(sapply(keys, function(key) str_interp('${key}: ${row[[key]]}')), collapse = ', ')

        cat(paste0(row$key, ': {', dict, '},\n'))
    }
}, file = 'output.txt')
