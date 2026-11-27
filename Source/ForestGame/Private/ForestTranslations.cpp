#include "ForestLanguage.h"

/**
 * Spanish and Italian for every English text of the game, found by the English
 * text itself.  To translate a new text, add a line with its English exactly as
 * written in the code (the same %s and %d, in the same order).
 */
namespace
{
	struct FTranslation
	{
		const TCHAR* English;
		const TCHAR* Spanish;
		const TCHAR* Italian;
	};

	const FTranslation GTable[] = {
		// --- Title screen and the wake-up -------------------------------------
		{TEXT("A sleeping forest is waiting for you to wake up."), TEXT("Un bosque dormido espera a que despiertes."), TEXT("Una foresta addormentata aspetta il tuo risveglio.")},
		{TEXT("Gather  ·  Befriend  ·  Build  ·  Survive the night"), TEXT("Recolecta  ·  Haz amigos  ·  Construye  ·  Sobrevive a la noche"), TEXT("Raccogli  ·  Fai amicizia  ·  Costruisci  ·  Sopravvivi alla notte")},
		{TEXT("PLAY"), TEXT("JUGAR"), TEXT("GIOCA")},
		{TEXT("The sun goes down. You lie down in the grass..."), TEXT("El sol se pone. Te tumbas en la hierba..."), TEXT("Il sole tramonta. Ti sdrai nell'erba...")},
		{TEXT("The night passes over the forest."), TEXT("La noche pasa sobre el bosque."), TEXT("La notte passa sulla foresta.")},
		{TEXT("The rain has stopped."), TEXT("La lluvia ha parado."), TEXT("La pioggia si è fermata.")},
		{TEXT("Somewhere above you, the leaves are still dripping..."), TEXT("En algún lugar sobre ti, las hojas todavía gotean..."), TEXT("Da qualche parte sopra di te, le foglie gocciolano ancora...")},
		{TEXT("The forest breathes slowly. So do you."), TEXT("El bosque respira despacio. Tú también."), TEXT("La foresta respira piano. Anche tu.")},
		{TEXT("Light slips between the branches..."), TEXT("La luz se cuela entre las ramas..."), TEXT("La luce scivola tra i rami...")},
		{TEXT("You open your eyes."), TEXT("Abres los ojos."), TEXT("Apri gli occhi.")},
		{TEXT("Slowly, you rise. The forest has been waiting for you."), TEXT("Despacio, te levantas. El bosque te esperaba."), TEXT("Piano piano, ti alzi. La foresta ti aspettava.")},
		{TEXT("[Space]  Skip"), TEXT("[Espacio]  Saltar"), TEXT("[Spazio]  Salta")},
		{TEXT("The rain has stopped. Morning rises between the pines..."), TEXT("La lluvia ha parado. La mañana se alza entre los pinos..."), TEXT("La pioggia si è fermata. Il mattino sorge tra i pini...")},
		{TEXT("You open your eyes, lying in the heart of the forest."), TEXT("Abres los ojos, tumbada en el corazón del bosque."), TEXT("Apri gli occhi, distesa nel cuore della foresta.")},
		{TEXT("You get up. The animals of the forest need you."), TEXT("Te levantas. Los animales del bosque te necesitan."), TEXT("Ti alzi. Gli animali della foresta hanno bisogno di te.")},
		{TEXT("The forest holds its breath..."), TEXT("El bosque contiene la respiración..."), TEXT("La foresta trattiene il respiro...")},
		{TEXT("Morning is here. Gather food and wood, win the animals' trust, build your camp (B)."), TEXT("Ya es de día. Recoge comida y madera, gánate la confianza de los animales, construye tu campamento (B)."), TEXT("È mattina. Raccogli cibo e legna, conquista la fiducia degli animali, costruisci il tuo accampamento (B).")},

		// --- Animals --------------------------------------------------------------
		{TEXT("eating"), TEXT("come"), TEXT("mangia")},
		{TEXT("hiding from the rain"), TEXT("se protege de la lluvia"), TEXT("si ripara dalla pioggia")},
		{TEXT("scared!"), TEXT("¡tiene miedo!"), TEXT("ha paura!")},
		{TEXT("looking for food"), TEXT("busca comida"), TEXT("cerca cibo")},
		{TEXT("curious"), TEXT("curioso"), TEXT("curioso")},
		{TEXT("stays at camp"), TEXT("se queda en el campamento"), TEXT("resta all'accampamento")},
		{TEXT("hungry: coming for you!"), TEXT("hambriento: ¡viene a por ti!"), TEXT("affamato: ti viene addosso!")},
		{TEXT("hungry: growls"), TEXT("hambriento: gruñe"), TEXT("affamato: ringhia")},
		{TEXT("hungry"), TEXT("tiene hambre"), TEXT("ha fame")},
		{TEXT("to "), TEXT("a "), TEXT("a ")},
		{TEXT("the shadow wolf"), TEXT("el lobo de las sombras"), TEXT("il lupo delle ombre")},
		{TEXT("the black boar"), TEXT("el jabalí negro"), TEXT("il cinghiale nero")},

		// --- Items and the build menu ------------------------------------------
		{TEXT("mushrooms"), TEXT("setas"), TEXT("funghi")},
		{TEXT("mushroom"), TEXT("seta"), TEXT("fungo")},
		{TEXT("berries"), TEXT("bayas"), TEXT("bacche")},
		{TEXT("berry"), TEXT("baya"), TEXT("bacca")},
		{TEXT("wood"), TEXT("madera"), TEXT("legna")},
		{TEXT("stones"), TEXT("piedras"), TEXT("pietre")},
		{TEXT("stone"), TEXT("piedra"), TEXT("pietra")},
		{TEXT("Campfire"), TEXT("Hoguera"), TEXT("Falò")},
		{TEXT("Light and warmth. Tame animals sleep by it at night."), TEXT("Luz y calor. Los animales domesticados duermen junto a ella."), TEXT("Luce e calore. Gli animali addomesticati ci dormono accanto.")},
		{TEXT("Feeder"), TEXT("Comedero"), TEXT("Mangiatoia")},
		{TEXT("Holds 6 berries. Hungry animals come to eat and learn to trust you."), TEXT("Guarda 6 bayas. Los animales hambrientos vienen a comer y aprenden a confiar en ti."), TEXT("Contiene 6 bacche. Gli animali affamati vengono a mangiare e imparano a fidarsi di te.")},
		{TEXT("Shelter"), TEXT("Refugio"), TEXT("Riparo")},
		{TEXT("A log roof. Animals hide under it from the rain."), TEXT("Un techo de troncos. Los animales se protegen de la lluvia."), TEXT("Un tetto di tronchi. Gli animali ci si riparano dalla pioggia.")},
		{TEXT("Fence"), TEXT("Valla"), TEXT("Recinto")},
		{TEXT("A piece of fence for your camp."), TEXT("Un trozo de valla para tu campamento."), TEXT("Un pezzo di recinto per il tuo accampamento.")},
		{TEXT("Lantern"), TEXT("Farol"), TEXT("Lanterna")},
		{TEXT("Lights up by itself at night."), TEXT("Se enciende sola por la noche."), TEXT("Si accende da sola di notte.")},
		{TEXT("Log bench"), TEXT("Banco de tronco"), TEXT("Panca di tronco")},
		{TEXT("A place to sit and watch the sky."), TEXT("Un sitio para sentarse a mirar el cielo."), TEXT("Un posto per sedersi a guardare il cielo.")},

		// --- The game manager ---------------------------------------------------
		{TEXT("Gather wood, stones and berries. Feed the animals until they trust you, and build your camp (B)."), TEXT("Recoge madera, piedras y bayas. Alimenta a los animales hasta que confíen en ti y construye tu campamento (B)."), TEXT("Raccogli legna, pietre e bacche. Nutri gli animali finché si fidano di te e costruisci il tuo accampamento (B).")},
		{TEXT("%s stays here at the camp."), TEXT("%s se queda aquí, en el campamento."), TEXT("%s resta qui, all'accampamento.")},
		{TEXT("%s follows you again."), TEXT("%s vuelve a seguirte."), TEXT("%s ti segue di nuovo.")},
		{TEXT("[E] Build here   [R] Turn   [Right click] Cancel"), TEXT("[E] Construir aquí   [R] Girar   [Clic derecho] Cancelar"), TEXT("[E] Costruisci qui   [R] Ruota   [Clic destro] Annulla")},
		{TEXT("Not here: find a flat, open spot   [Right click] Cancel"), TEXT("Aquí no: busca un sitio llano y despejado   [Clic derecho] Cancelar"), TEXT("Non qui: trova un posto piano e libero   [Clic destro] Annulla")},
		{TEXT("[E]  Pick up %s"), TEXT("[E]  Recoger: %s"), TEXT("[E]  Raccogli: %s")},
		{TEXT("[E]  Give %s %s   [F] Stay / follow"), TEXT("[E]  Dar %s %s   [F] Quieto / sígueme"), TEXT("[E]  Dai %s %s   [F] Resta / seguimi")},
		{TEXT("[E]  Pet %s   [F] Stay / follow"), TEXT("[E]  Acariciar: %s   [F] Quieto / sígueme"), TEXT("[E]  Accarezza %s   [F] Resta / seguimi")},
		{TEXT("%s would like %s"), TEXT("%s quiere: %s"), TEXT("%s vorrebbe: %s")},
		{TEXT("[E]  Give %s %s%s"), TEXT("[E]  Dar %s %s%s"), TEXT("[E]  Dai %s %s%s")},
		{TEXT("  (its favourite!)"), TEXT("  (¡su favorito!)"), TEXT("  (il suo preferito!)")},
		{TEXT("[E]  Fill the feeder with berries (%d / %d)"), TEXT("[E]  Llenar el comedero de bayas (%d / %d)"), TEXT("[E]  Riempi la mangiatoia di bacche (%d / %d)")},
		{TEXT("The feeder needs berries (%d / %d)"), TEXT("El comedero necesita bayas (%d / %d)"), TEXT("La mangiatoia ha bisogno di bacche (%d / %d)")},
		{TEXT("+%s  (%d in your basket)"), TEXT("+%s  (%d en tu cesta)"), TEXT("+%s  (%d nel tuo cesto)")},
		{TEXT("%s loves that."), TEXT("¡%s está encantado!"), TEXT("%s è felicissimo!")},
		{TEXT("%s trusts you now and follows you! (%d / %d tame)"), TEXT("¡%s ya confía en ti y te sigue! (%d / %d domesticados)"), TEXT("%s ora si fida di te e ti segue! (%d / %d addomesticati)")},
		{TEXT("Yum, its favourite! %s trusts you more (%d%%)."), TEXT("¡Ñam, su favorito! %s confía más en ti (%d%%)."), TEXT("Gnam, il suo preferito! %s si fida di più (%d%%).")},
		{TEXT("Yum! %s trusts you a little more (%d%%)."), TEXT("¡Ñam! %s confía un poco más en ti (%d%%)."), TEXT("Gnam! %s si fida un po' di più (%d%%).")},
		{TEXT("%s eats happily."), TEXT("%s come con gusto."), TEXT("%s mangia contento.")},
		{TEXT("You have nothing %s eats... It loves %s."), TEXT("No tienes nada que dar %s... Le encantan: %s."), TEXT("Non hai niente da dare %s... Adora: %s.")},
		{TEXT("You put %s in the feeder."), TEXT("Pones %s en el comedero."), TEXT("Metti %s nella mangiatoia.")},
		{TEXT("Every animal of the forest trusts you! The forest thanks you."), TEXT("¡Todos los animales del bosque confían en ti! El bosque te da las gracias."), TEXT("Tutti gli animali della foresta si fidano di te! La foresta ti ringrazia.")},
		{TEXT("%s needs %s."), TEXT("%s: hace falta %s."), TEXT("%s: servono %s.")},
		{TEXT("Not here: find a flat, open spot."), TEXT("Aquí no: busca un sitio llano y despejado."), TEXT("Non qui: trova un posto piano e libero.")},
		{TEXT("%s built!"), TEXT("¡%s construido!"), TEXT("%s costruito!")},
		{TEXT("A fresh camp: basket, buildings and the animals' trust are cleared."), TEXT("Campamento nuevo: cesta, construcciones y confianza de los animales borradas."), TEXT("Nuovo accampamento: cesto, costruzioni e fiducia degli animali cancellati.")},

		// --- The HUD ----------------------------------------------------------------
		{TEXT("Animals tamed: %d / %d"), TEXT("Animales domesticados: %d / %d"), TEXT("Animali addomesticati: %d / %d")},
		{TEXT("BUILD  —  press a number   [B] Close"), TEXT("CONSTRUIR  —  pulsa un número   [B] Cerrar"), TEXT("COSTRUISCI  —  premi un numero   [B] Chiudi")},
		{TEXT("[E] Pick up / feed / pet"), TEXT("[E] Recoger / alimentar / acariciar"), TEXT("[E] Raccogli / nutri / accarezza")},
		{TEXT("[F] Tame animal: stay / follow"), TEXT("[F] Animal domesticado: quieto / sígueme"), TEXT("[F] Animale addomesticato: resta / seguimi")},
		{TEXT("[B] Build"), TEXT("[B] Construir"), TEXT("[B] Costruisci")},
		{TEXT("[H] Hide this help   [F9] New camp"), TEXT("[H] Ocultar ayuda   [F9] Campamento nuevo"), TEXT("[H] Nascondi l'aiuto   [F9] Nuovo accampamento")},
		{TEXT("Day %d  -  %02d:%02d  -  %s"), TEXT("Día %d  -  %02d:%02d  -  %s"), TEXT("Giorno %d  -  %02d:%02d  -  %s")},

		// --- Night, survival, seasons -----------------------------------------
		{TEXT("Night falls... Something is moving in the dark. Stay close to a campfire!"), TEXT("Cae la noche... Algo se mueve en la oscuridad. ¡Quédate cerca de una hoguera!"), TEXT("Scende la notte... Qualcosa si muove nel buio. Resta vicino a un falò!")},
		{TEXT("Dawn! The creatures of the night flee into the trees."), TEXT("¡El alba! Las criaturas de la noche huyen entre los árboles."), TEXT("L'alba! Le creature della notte fuggono tra gli alberi.")},
		{TEXT("Red eyes in the dark... %s is hunting you. Run to a fire!"), TEXT("Ojos rojos en la oscuridad... %s te está cazando. ¡Corre hacia un fuego!"), TEXT("Occhi rossi nel buio... %s ti sta cacciando. Corri verso un fuoco!")},
		{TEXT("You fainted...\nThe forest is dangerous when its animals are hungry."), TEXT("Te has desmayado...\nEl bosque es peligroso cuando sus animales tienen hambre."), TEXT("Sei svenuta...\nLa foresta è pericolosa quando i suoi animali hanno fame.")},
		{TEXT("%s is hungry and growls... Feed it or step back!"), TEXT("%s tiene hambre y gruñe... ¡Dale de comer o retrocede!"), TEXT("%s ha fame e ringhia... Dagli da mangiare o allontanati!")},
		{TEXT("Ouch! %s bit you. Give it some food!"), TEXT("¡Ay! %s te ha mordido. ¡Dale algo de comer!"), TEXT("Ahi! %s ti ha morso. Dagli qualcosa da mangiare!")},
		{TEXT("You wake up by your campfire, rested."), TEXT("Te despiertas junto a tu hoguera, descansada."), TEXT("Ti svegli accanto al tuo falò, riposata.")},
		{TEXT("You wake up where your journey began. Build a campfire to wake up there."), TEXT("Te despiertas donde empezó tu viaje. Construye una hoguera para despertar allí."), TEXT("Ti svegli dove è iniziato il tuo viaggio. Costruisci un falò per svegliarti lì.")},
		{TEXT("Spring"), TEXT("Primavera"), TEXT("Primavera")},
		{TEXT("Summer"), TEXT("Verano"), TEXT("Estate")},
		{TEXT("Autumn"), TEXT("Otoño"), TEXT("Autunno")},
		{TEXT("Winter"), TEXT("Invierno"), TEXT("Inverno")},
		{TEXT("Spring is here: the forest turns green again."), TEXT("Llega la primavera: el bosque vuelve a ser verde."), TEXT("È primavera: la foresta torna verde.")},
		{TEXT("Summer: long warm days."), TEXT("Verano: días largos y cálidos."), TEXT("Estate: lunghe giornate calde.")},
		{TEXT("Autumn: the leaves turn gold and red, the animals fill up before winter."), TEXT("Otoño: las hojas se vuelven doradas y rojas, los animales comen antes del invierno."), TEXT("Autunno: le foglie diventano d'oro e rosse, gli animali si riempiono prima dell'inverno.")},
		{TEXT("Winter: snow is coming and food is scarce. Hungry animals are dangerous!"), TEXT("Invierno: llega la nieve y escasea la comida. ¡Los animales hambrientos son peligrosos!"), TEXT("Inverno: arriva la neve e il cibo scarseggia. Gli animali affamati sono pericolosi!")},
	};

	const TMap<FString, const FTranslation*>& Index()
	{
		static TMap<FString, const FTranslation*> Map;
		if (Map.IsEmpty())
		{
			for (const FTranslation& Entry : GTable)
			{
				Map.Add(Entry.English, &Entry);
			}
		}
		return Map;
	}
}

const TCHAR* ForestTranslateWorld(const TCHAR* English, EForestLanguage Language);

const TCHAR* ForestLanguage::Translate(const TCHAR* English, EForestLanguage Language)
{
	if (!English)
	{
		return nullptr;
	}
	if (Language != EForestLanguage::Spanish && Language != EForestLanguage::Italian)
	{
		// Portuguese, Chinese, Japanese: ForestTranslationsWorld.cpp.
		return ForestTranslateWorld(English, Language);
	}
	const FTranslation* const* Found = Index().Find(English);
	if (!Found)
	{
		return nullptr;
	}
	return Language == EForestLanguage::Spanish ? (*Found)->Spanish : Language == EForestLanguage::Italian ? (*Found)->Italian : nullptr;
}
