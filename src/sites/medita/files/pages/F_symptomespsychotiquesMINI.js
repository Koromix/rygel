data.makeFormHeader("Questionnaire de symptômes psychotiques (MINI)", page)


form.pushOptions({mandatory: true, missingMode: 'disable'})

form.binary("a", "1. Avez-vous déjà eu l’impression que quelqu’un vous espionnait, ou complotait contre vous, ou bien encore que l’on essayait de vous faire du mal ?")
form.binary("b", "2. Avez-vous déjà eu l’impression que l’on pouvait lire ou entendre vos pensées ou que vous pouviez lire ou entendre les pensées des autres ?")
form.binary("c", "3. Avez-vous déjà cru que quelqu’un ou que quelque chose d’extérieur à vous introduisait dans votre tête des pensées étranges qui n’étaient pas les vôtres ou vous faisait agir d’une façon inhabituelle pour vous ? Avez-vous déjà eu l’impression d’être possédé ?")
form.binary("d", "4. Avez-vous déjà eu l’impression que l’on s’adressait directement à vous à travers la télévision ou la radio ou que certaines personnes que vous ne connaissiez pas personnellement s’intéressaient particulièrement à vous ?")
form.binary("e", "5. Avez-vous déjà eu des idées que vos proches considéraient comme étranges ou hors de la réalité, et qu’ils ne partageaient pas avec vous ?")
form.binary("f", "6. Vous est-il déjà arrivé d’entendre des choses que d’autres personnes ne pouvaient pas entendre, comme des voix ?")
form.binary("g", "7. Vous est-il déjà arrivé alors que vous étiez éveillé(e), d’avoir des visions ou de voir des choses que d’autres personnes ne pouvaient pas voir ?")



let score = form.value("a") +
            form.value("b") +
            form.value("c") +
            form.value("d") +
            form.value("e") +
            form.value("f") +
            form.value("g")
form.calc("score", "Score total", score)

data.makeFormFooter(page)
