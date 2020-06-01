data.makeFormHeader("CTQ", page)

form.pushOptions({mandatory: true, missingMode: 'disable'})

let dataQSpecial = [[5, "Jamais"], [4 , "Rarement"],[3, "Quelquefois"], [2, "Souvent"], [1, "Très souvent"]]
let dataQ = [[1, "Jamais"], [2, "Rarement"],[3, "Quelquefois"],[4, "Souvent"],[5, "Très souvent"]]

form.enum("Q1", "1. Il m’est arrivé de ne pas avoir assez à manger.",dataQ)
form.enum("Q2", "2. Je savais qu’il y avait quelqu’un pour prendre soin de moi et me protéger.", dataQSpecial)
form.enum("Q3", "3. Des membres de ma famille me disaient que j’étais « stupide » ou « paresseux » ou « laid ».", dataQ)
form.enum("Q4", "4. Mes parents étaient trop saouls ou « pétés » pour s’occuper de la famille.", dataQ)
form.enum("Q5", "5. Il y avait quelqu'un dans ma famille qui m’aidait à sentir que j’étais important ou particulier.", dataQSpecial)
form.enum("Q6", "6. Je devais porter des vêtements sales.",dataQ)
form.enum("Q7", "7.Je me sentais aimé(e).",dataQSpecial)
form.enum("Q8", "8. Je pensais que mes parents n’avaient pas souhaité ma naissance",dataQ)
form.enum("Q9", "9. J’ai été frappé(e) si fort par un membre de ma famille que j’ai dû consulter un docteur ou aller à l’hôpital.", dataQ)
form.enum("Q10", "10. Je n’aurais rien voulu changer à ma famille. ", dataQ)
form.enum("Q11", "11. Un membre de ma famille m’a frappé(e) si fort que j’ai eu des bleus ou des marques.", dataQ)
form.enum("Q12", "12. J’étais puni(e) au moyen d’une ceinture, d’un bâton, d’une corde ou de quelque autre objet dur.", dataQ)
form.enum("Q13", "13 .Les membres de ma famille étaient attentifs les uns aux autres.", dataQSpecial)
form.enum("Q14", "14. Les membres de ma famille me disaient des choses blessantes ou insultantes.", dataQ)
form.enum("Q15", "15.Je pense que j’ai été physiquement maltraité(e).", dataQ)
form.enum("Q16", "16. J’ai eu une enfance parfaite.", dataQ)
form.enum("Q17", "17. J’ai été frappé(e) ou battu(e) si fort que quelqu’un l’a remarqué (par ex. un professeur, un voisin ou un docteur).", dataQ)
form.enum("Q18", "18. J’avais le sentiment que quelqu’un dans ma famille me détestait.", dataQ)
form.enum("Q19", "19. Les membres de ma famille se sentaient proches les uns des autres.", dataQSpecial)
form.enum("Q20", "20. Quelqu’un a essayé de me faire des attouchements sexuels ou de m’en faire faire.", dataQ)
form.enum("Q21", "21. Quelqu’un a menacé de me blesser ou de raconter des mensonges à mon sujet si je ne faisais pas quelque chose de nature sexuelle avec lui.", dataQ)
form.enum("Q22", "22. J’avais la meilleure famille du monde.", dataQ)
form.enum("Q23", "23. Quelqu’un a essayé de me faire faire des actes sexuels ou de me faire regarder de tels actes", dataQ)
form.enum("Q24", "24. J’ai été victime d’abus sexuels.", dataQ)
form.enum("Q25", "25. Je pense que j’ai été maltraité affectivement.", dataQ)
form.enum("Q26", "26. Il y avait quelqu’un pour m’emmener chez le docteur si j’en avais besoin.", dataQSpecial)
form.enum("Q27", "27. Je pense qu’on a abusé de moi sexuellement", dataQ)
form.enum("Q28", "28. Ma famille était une source de force et de soutien.", dataQSpecial)

form.calc("physicalAbuse", "Physical abuse", form.value("Q9") + form.value("Q11") + form.value("Q12") +
                                             form.value("Q15") + form.value("Q17"));
form.calc("emotionalNeglect", "Emotional Neglect", form.value("Q5") + form.value("Q7") + form.value("Q13") +
                                                   form.value("Q19") + form.value("Q28"));
form.calc("physicalNeglect", "Physical Neglect", form.value("Q1") + form.value("Q2") + form.value("Q4") +
                                                 form.value("Q6") + form.value("Q26"));
form.calc("sexualAbuse", "Sexual abuse", form.value("Q20") + form.value("Q21") + form.value("Q23") +
                                         form.value("Q24") + form.value("Q27"));
form.calc("emotionalAbuse", "Emotional Abuse", form.value("Q3") + form.value("Q8") + form.value("Q14") +
                                              form.value("Q18") + form.value("Q25"));

let score = form.value("physicalAbuse") +
            form.value("emotionalNeglect") +
            form.value("physicalNeglect") +
            form.value("sexualAbuse") +
            form.value("emotionalAbuse")
form.calc("score", "Score total", score);

data.makeFormFooter(nav, page)
