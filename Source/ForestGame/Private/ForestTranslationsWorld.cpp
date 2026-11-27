#include "ForestLanguage.h"

/**
 * Portuguese, Chinese (simplified) and Japanese for every English text of the
 * game, found by the English text itself (as in ForestTranslations.cpp).  The
 * Chinese and Japanese use the engine's fallback font, which has their
 * characters.  To translate a new text, add a line with its English exactly
 * as written in the code (the same %s and %d, in the same order).
 */
namespace
{
	struct FWorldTranslation
	{
		const TCHAR* English;
		const TCHAR* Portuguese;
		const TCHAR* Chinese;
		const TCHAR* Japanese;
	};

	const FWorldTranslation GWorldTable[] = {
		// --- Title screen and the wake-up -------------------------------------
		{TEXT("A sleeping forest is waiting for you to wake up."), TEXT("Uma floresta adormecida espera você acordar."), TEXT("一片沉睡的森林正等待你醒来。"), TEXT("眠れる森が、あなたの目覚めを待っている。")},
		{TEXT("Gather  ·  Befriend  ·  Build  ·  Survive the night"), TEXT("Colete  ·  Faça amigos  ·  Construa  ·  Sobreviva à noite"), TEXT("采集  ·  结交  ·  建造  ·  熬过黑夜"), TEXT("集める  ·  仲良くなる  ·  建てる  ·  夜を生き延びる")},
		{TEXT("PLAY"), TEXT("JOGAR"), TEXT("开始"), TEXT("はじめる")},
		{TEXT("The sun goes down. You lie down in the grass..."), TEXT("O sol se põe. Você se deita na grama..."), TEXT("太阳落山了。你在草地上躺下……"), TEXT("日が沈む。あなたは草の上に横たわる……")},
		{TEXT("The night passes over the forest."), TEXT("A noite passa sobre a floresta."), TEXT("夜色漫过森林。"), TEXT("夜が森の上を過ぎていく。")},
		{TEXT("The rain has stopped."), TEXT("A chuva parou."), TEXT("雨停了。"), TEXT("雨がやんだ。")},
		{TEXT("Somewhere above you, the leaves are still dripping..."), TEXT("Em algum lugar acima de você, as folhas ainda pingam..."), TEXT("头顶的某处，树叶还在滴水……"), TEXT("どこか頭上で、葉がまだしずくを落としている……")},
		{TEXT("The forest breathes slowly. So do you."), TEXT("A floresta respira devagar. Você também."), TEXT("森林在缓缓呼吸。你也一样。"), TEXT("森はゆっくりと息をしている。あなたも。")},
		{TEXT("Light slips between the branches..."), TEXT("A luz escorrega entre os galhos..."), TEXT("光从枝叶间悄悄溜进来……"), TEXT("枝のあいだから光がこぼれてくる……")},
		{TEXT("You open your eyes."), TEXT("Você abre os olhos."), TEXT("你睁开了眼睛。"), TEXT("あなたは目を開ける。")},
		{TEXT("Slowly, you rise. The forest has been waiting for you."), TEXT("Devagar, você se levanta. A floresta esperava por você."), TEXT("你慢慢站起身。森林一直在等你。"), TEXT("ゆっくりと、あなたは起き上がる。森はずっと待っていた。")},
		{TEXT("[Space]  Skip"), TEXT("[Espaço]  Pular"), TEXT("[空格]  跳过"), TEXT("[スペース]  スキップ")},
		{TEXT("Morning is here. Gather food and wood, win the animals' trust, build your camp (B)."), TEXT("A manhã chegou. Colete comida e madeira, ganhe a confiança dos animais, construa seu acampamento (B)."), TEXT("早晨来了。收集食物和木头，赢得动物的信任，建造你的营地（B）。"), TEXT("朝が来た。食べ物と木を集め、動物たちの信頼を得て、キャンプを作ろう（B）。")},

		// --- Animals --------------------------------------------------------------
		{TEXT("eating"), TEXT("comendo"), TEXT("在吃东西"), TEXT("食事中")},
		{TEXT("hiding from the rain"), TEXT("se abrigando da chuva"), TEXT("在躲雨"), TEXT("雨宿り中")},
		{TEXT("scared!"), TEXT("com medo!"), TEXT("害怕！"), TEXT("こわがっている！")},
		{TEXT("looking for food"), TEXT("procurando comida"), TEXT("在找吃的"), TEXT("食べ物を探している")},
		{TEXT("curious"), TEXT("curioso"), TEXT("好奇"), TEXT("興味しんしん")},
		{TEXT("stays at camp"), TEXT("fica no acampamento"), TEXT("留在营地"), TEXT("キャンプで待機")},
		{TEXT("hungry: coming for you!"), TEXT("faminto: vem atrás de você!"), TEXT("饿了：冲你来了！"), TEXT("空腹：こっちに向かってくる！")},
		{TEXT("hungry: growls"), TEXT("faminto: rosna"), TEXT("饿了：在低吼"), TEXT("空腹：うなっている")},
		{TEXT("hungry"), TEXT("com fome"), TEXT("饿了"), TEXT("おなかがすいている")},
		{TEXT("to "), TEXT("para "), TEXT(""), TEXT("")},
		{TEXT("the shadow wolf"), TEXT("o lobo das sombras"), TEXT("暗影之狼"), TEXT("影の狼")},
		{TEXT("the black boar"), TEXT("o javali negro"), TEXT("黑色野猪"), TEXT("黒いイノシシ")},

		// --- Items and the build menu ------------------------------------------
		{TEXT("mushrooms"), TEXT("cogumelos"), TEXT("蘑菇"), TEXT("キノコ")},
		{TEXT("mushroom"), TEXT("cogumelo"), TEXT("蘑菇"), TEXT("キノコ")},
		{TEXT("berries"), TEXT("bagas"), TEXT("浆果"), TEXT("ベリー")},
		{TEXT("berry"), TEXT("baga"), TEXT("浆果"), TEXT("ベリー")},
		{TEXT("wood"), TEXT("madeira"), TEXT("木头"), TEXT("木")},
		{TEXT("stones"), TEXT("pedras"), TEXT("石头"), TEXT("石")},
		{TEXT("stone"), TEXT("pedra"), TEXT("石头"), TEXT("石")},
		{TEXT("Campfire"), TEXT("Fogueira"), TEXT("篝火"), TEXT("たき火")},
		{TEXT("Light and warmth. Tame animals sleep by it at night."), TEXT("Luz e calor. Animais domesticados dormem ao lado dela à noite."), TEXT("光和温暖。被驯服的动物夜里会睡在旁边。"), TEXT("光とぬくもり。なついた動物は夜その横で眠る。")},
		{TEXT("Feeder"), TEXT("Comedouro"), TEXT("喂食槽"), TEXT("えさ箱")},
		{TEXT("Holds 6 berries. Hungry animals come to eat and learn to trust you."), TEXT("Guarda 6 bagas. Animais famintos vêm comer e aprendem a confiar em você."), TEXT("可放6颗浆果。饥饿的动物会来吃，并学会信任你。"), TEXT("ベリーが6個入る。おなかをすかせた動物が食べに来て、あなたを信頼するようになる。")},
		{TEXT("Shelter"), TEXT("Abrigo"), TEXT("庇护所"), TEXT("雨よけ小屋")},
		{TEXT("A log roof. Animals hide under it from the rain."), TEXT("Um telhado de troncos. Os animais se abrigam da chuva embaixo dele."), TEXT("原木屋顶。动物们会在下面躲雨。"), TEXT("丸太の屋根。動物たちがその下で雨をしのぐ。")},
		{TEXT("Fence"), TEXT("Cerca"), TEXT("栅栏"), TEXT("柵")},
		{TEXT("A piece of fence for your camp."), TEXT("Um pedaço de cerca para o seu acampamento."), TEXT("给营地用的一段栅栏。"), TEXT("キャンプ用の柵ひとつ。")},
		{TEXT("Lantern"), TEXT("Lanterna"), TEXT("灯笼"), TEXT("ランタン")},
		{TEXT("Lights up by itself at night."), TEXT("Acende sozinha à noite."), TEXT("夜里会自己亮起来。"), TEXT("夜になるとひとりでに灯る。")},
		{TEXT("Log bench"), TEXT("Banco de tronco"), TEXT("原木长凳"), TEXT("丸太のベンチ")},
		{TEXT("A place to sit and watch the sky."), TEXT("Um lugar para sentar e olhar o céu."), TEXT("坐下来看看天空的地方。"), TEXT("座って空をながめる場所。")},

		// --- Natural events -------------------------------------------------------
		{TEXT("The sky darkens... A storm is coming. Watch out for lightning!"), TEXT("O céu escurece... Uma tempestade se aproxima. Cuidado com os raios!"), TEXT("天色暗了下来……暴风雨要来了。小心闪电！"), TEXT("空が暗くなる……嵐が来る。雷に気をつけて！")},
		{TEXT("A storm broke over the forest."), TEXT("Uma tempestade caiu sobre a floresta."), TEXT("暴风雨席卷了森林。"), TEXT("森に嵐が吹き荒れた。")},
		{TEXT("The air crackles... Lightning is about to strike. Move!"), TEXT("O ar estala... Um raio vai cair. Saia daí!"), TEXT("空气噼啪作响……闪电要劈下来了。快跑！"), TEXT("空気がぱちぱち鳴る……雷が落ちる。動いて！")},
		{TEXT("Struck by lightning!"), TEXT("Atingida por um raio!"), TEXT("被闪电击中了！"), TEXT("雷に打たれた！")},
		{TEXT("Lightning struck right next to you."), TEXT("Um raio caiu bem ao seu lado."), TEXT("闪电就劈在你身边。"), TEXT("すぐそばに雷が落ちた。")},
		{TEXT("Lightning set the grass on fire nearby."), TEXT("Um raio pôs fogo na grama aqui perto."), TEXT("闪电点燃了附近的草地。"), TEXT("雷で近くの草が燃え出した。")},
		{TEXT("The ground is shaking! Earthquake!"), TEXT("O chão está tremendo! Terremoto!"), TEXT("地面在摇晃！地震了！"), TEXT("地面が揺れている！地震だ！")},
		{TEXT("The earthquake uncovered a buried chest nearby..."), TEXT("O terremoto revelou um baú enterrado aqui perto..."), TEXT("地震让附近一个埋着的箱子露了出来……"), TEXT("地震で近くに埋まっていた宝箱が出てきた……")},
		{TEXT("An earthquake shook the forest and uncovered a chest."), TEXT("Um terremoto sacudiu a floresta e revelou um baú."), TEXT("一场地震摇晃了森林，露出一个箱子。"), TEXT("地震が森を揺らし、宝箱が現れた。")},
		{TEXT("An earthquake shook the forest."), TEXT("Um terremoto sacudiu a floresta."), TEXT("一场地震摇晃了森林。"), TEXT("地震が森を揺らした。")},
		{TEXT("A shooting star! It is falling into the forest..."), TEXT("Uma estrela cadente! Ela está caindo na floresta..."), TEXT("流星！它正坠入森林……"), TEXT("流れ星！森に落ちていく……")},
		{TEXT("The star landed nearby. Something is shining over there!"), TEXT("A estrela caiu aqui perto. Algo está brilhando ali!"), TEXT("星星落在附近。那边有东西在发光！"), TEXT("星が近くに落ちた。あそこで何かが光っている！")},
		{TEXT("A shooting star fell into the forest."), TEXT("Uma estrela cadente caiu na floresta."), TEXT("一颗流星坠入了森林。"), TEXT("流れ星が森に落ちた。")},

		// --- The game manager ---------------------------------------------------
		{TEXT("Gather wood, stones and berries. Feed the animals until they trust you, and build your camp (B)."), TEXT("Colete madeira, pedras e bagas. Alimente os animais até confiarem em você e construa seu acampamento (B)."), TEXT("收集木头、石头和浆果。喂养动物直到它们信任你，并建造你的营地（B）。"), TEXT("木と石とベリーを集めよう。動物が信頼してくれるまでえさをあげて、キャンプを作ろう（B）。")},
		{TEXT("%s stays here at the camp."), TEXT("%s fica aqui no acampamento."), TEXT("%s留在营地这里。"), TEXT("%sはキャンプに残る。")},
		{TEXT("%s follows you again."), TEXT("%s volta a seguir você."), TEXT("%s又跟着你了。"), TEXT("%sはまたついてくる。")},
		{TEXT("[E] Build here   [R] Turn   [Right click] Cancel"), TEXT("[E] Construir aqui   [R] Girar   [Clique direito] Cancelar"), TEXT("[E] 在此建造   [R] 旋转   [右键] 取消"), TEXT("[E] ここに建てる   [R] 回転   [右クリック] キャンセル")},
		{TEXT("Not here: find a flat, open spot   [Right click] Cancel"), TEXT("Aqui não: procure um lugar plano e livre   [Clique direito] Cancelar"), TEXT("这里不行：找一块平坦空旷的地方   [右键] 取消"), TEXT("ここは無理：平らで開けた場所を探して   [右クリック] キャンセル")},
		{TEXT("[E]  Pick up %s"), TEXT("[E]  Pegar: %s"), TEXT("[E]  拾取：%s"), TEXT("[E]  拾う：%s")},
		{TEXT("[E]  Give %s %s   [F] Stay / follow"), TEXT("[E]  Dar %s %s   [F] Fica / segue"), TEXT("[E]  喂食：%s → %s   [F] 留下 / 跟随"), TEXT("[E]  あげる：%s → %s   [F] 待て / ついて来て")},
		{TEXT("[E]  Pet %s   [F] Stay / follow"), TEXT("[E]  Fazer carinho: %s   [F] Fica / segue"), TEXT("[E]  抚摸%s   [F] 留下 / 跟随"), TEXT("[E]  なでる：%s   [F] 待て / ついて来て")},
		{TEXT("%s would like %s"), TEXT("%s gostaria de: %s"), TEXT("%s想要：%s"), TEXT("%sは%sがほしい")},
		{TEXT("[E]  Give %s %s%s"), TEXT("[E]  Dar %s %s%s"), TEXT("[E]  喂食：%s → %s%s"), TEXT("[E]  あげる：%s → %s%s")},
		{TEXT("  (its favourite!)"), TEXT("  (o favorito dele!)"), TEXT("  （它的最爱！）"), TEXT("  （大好物！）")},
		{TEXT("[E]  Fill the feeder with berries (%d / %d)"), TEXT("[E]  Encher o comedouro com bagas (%d / %d)"), TEXT("[E]  用浆果装满喂食槽（%d / %d）"), TEXT("[E]  えさ箱にベリーを入れる（%d / %d）")},
		{TEXT("The feeder needs berries (%d / %d)"), TEXT("O comedouro precisa de bagas (%d / %d)"), TEXT("喂食槽需要浆果（%d / %d）"), TEXT("えさ箱にはベリーが必要（%d / %d）")},
		{TEXT("+%s  (%d in your basket)"), TEXT("+%s  (%d na sua cesta)"), TEXT("+%s  （篮子里有%d）"), TEXT("+%s  （かごに%d）")},
		{TEXT("%s loves that."), TEXT("%s adora isso."), TEXT("%s很喜欢。"), TEXT("%sはうれしそう。")},
		{TEXT("You have nothing %s eats... It loves %s."), TEXT("Você não tem nada para dar %s... Ele adora: %s."), TEXT("你没有能给%s吃的……它最爱：%s。"), TEXT("%sにあげるものがない……大好物は%s。")},
		{TEXT("%s trusts you now and follows you! (%d / %d tame)"), TEXT("%s agora confia em você e te segue! (%d / %d domesticados)"), TEXT("%s现在信任你，跟着你走了！（已驯服 %d / %d）"), TEXT("%sはあなたを信頼して、ついてくる！（%d / %d 頭がなついた）")},
		{TEXT("Yum, its favourite! %s trusts you more (%d%%)."), TEXT("Hum, o favorito dele! %s confia mais em você (%d%%)."), TEXT("好吃，是它的最爱！%s更信任你了（%d%%）。"), TEXT("おいしい、大好物！%sの信頼が深まった（%d%%）。")},
		{TEXT("Yum! %s trusts you a little more (%d%%)."), TEXT("Hum! %s confia um pouco mais em você (%d%%)."), TEXT("好吃！%s更信任你一点了（%d%%）。"), TEXT("おいしい！%sが少し信頼してくれた（%d%%）。")},
		{TEXT("%s eats happily."), TEXT("%s come feliz."), TEXT("%s吃得很开心。"), TEXT("%sはうれしそうに食べる。")},
		{TEXT("You put %s in the feeder."), TEXT("Você coloca %s no comedouro."), TEXT("你把%s放进了喂食槽。"), TEXT("えさ箱に%sを入れた。")},
		{TEXT("Every animal of the forest trusts you! The forest thanks you."), TEXT("Todos os animais da floresta confiam em você! A floresta agradece."), TEXT("森林里所有的动物都信任你了！森林感谢你。"), TEXT("森のすべての動物があなたを信頼している！森がありがとうと言っている。")},
		{TEXT("%s needs %s."), TEXT("%s: precisa de %s."), TEXT("%s：需要%s。"), TEXT("%s：%sが必要。")},
		{TEXT("Not here: find a flat, open spot."), TEXT("Aqui não: procure um lugar plano e livre."), TEXT("这里不行：找一块平坦空旷的地方。"), TEXT("ここは無理：平らで開けた場所を探して。")},
		{TEXT("%s built!"), TEXT("%s construído!"), TEXT("%s建好了！"), TEXT("%sができた！")},
		{TEXT("A fresh camp: basket, buildings and the animals' trust are cleared."), TEXT("Acampamento novo: cesta, construções e confiança dos animais apagadas."), TEXT("新的营地：篮子、建筑和动物的信任都已清空。"), TEXT("新しいキャンプ：かご、建物、動物の信頼がリセットされた。")},

		// --- The HUD ----------------------------------------------------------------
		{TEXT("Animals tamed: %d / %d"), TEXT("Animais domesticados: %d / %d"), TEXT("已驯服动物：%d / %d"), TEXT("なついた動物：%d / %d")},
		{TEXT("BUILD  —  press a number   [B] Close"), TEXT("CONSTRUIR  —  aperte um número   [B] Fechar"), TEXT("建造  —  按数字键   [B] 关闭"), TEXT("建てる  —  数字キーを押す   [B] 閉じる")},
		{TEXT("[E] Pick up / feed / pet"), TEXT("[E] Pegar / alimentar / fazer carinho"), TEXT("[E] 拾取 / 喂食 / 抚摸"), TEXT("[E] 拾う / えさをあげる / なでる")},
		{TEXT("[F] Tame animal: stay / follow"), TEXT("[F] Animal domesticado: fica / segue"), TEXT("[F] 驯服的动物：留下 / 跟随"), TEXT("[F] なついた動物：待て / ついて来て")},
		{TEXT("[B] Build"), TEXT("[B] Construir"), TEXT("[B] 建造"), TEXT("[B] 建てる")},
		{TEXT("[H] Hide this help   [F9] New camp"), TEXT("[H] Esconder a ajuda   [F9] Novo acampamento"), TEXT("[H] 隐藏帮助   [F9] 新营地"), TEXT("[H] ヘルプを隠す   [F9] 新しいキャンプ")},
		{TEXT("Day %d  -  %02d:%02d  -  %s"), TEXT("Dia %d  -  %02d:%02d  -  %s"), TEXT("第%d天  -  %02d:%02d  -  %s"), TEXT("%d日目  -  %02d:%02d  -  %s")},

		// --- Survival life: hunger, tools, chests, garden, calendar ------------
		{TEXT("Starving!"), TEXT("Faminta!"), TEXT("饿坏了！"), TEXT("空腹で倒れそう！")},
		{TEXT("Food"), TEXT("Comida"), TEXT("食物"), TEXT("食料")},
		{TEXT("Survival journal   [J]"), TEXT("Diário de sobrevivência   [J]"), TEXT("生存日记   [J]"), TEXT("サバイバル日記   [J]")},
		{TEXT("You woke up alone in the forest."), TEXT("Você acordou sozinha na floresta."), TEXT("你独自在森林里醒来。"), TEXT("あなたは森の中でひとり目を覚ました。")},
		{TEXT("You are getting very hungry... Eat something (C)!"), TEXT("Você está ficando com muita fome... Coma algo (C)!"), TEXT("你越来越饿了……吃点东西（C）！"), TEXT("とてもおなかがすいてきた……何か食べよう（C）！")},
		{TEXT("You are starving! Eat something (C)."), TEXT("Você está morrendo de fome! Coma algo (C)."), TEXT("你快饿死了！吃点东西（C）。"), TEXT("飢えている！何か食べよう（C）。")},
		{TEXT("You are not hungry."), TEXT("Você não está com fome."), TEXT("你不饿。"), TEXT("おなかはすいていない。")},
		{TEXT("Your basket has nothing to eat. Find berries or mushrooms, or grow some!"), TEXT("Sua cesta não tem nada para comer. Encontre bagas ou cogumelos, ou plante alguns!"), TEXT("篮子里没有吃的。去找浆果或蘑菇，或者自己种！"), TEXT("かごに食べ物がない。ベリーかキノコを探すか、育てよう！")},
		{TEXT("You grill a %s over the fire. Delicious!"), TEXT("Você assa %s no fogo. Delicioso!"), TEXT("你在火上烤了%s。真好吃！"), TEXT("%sを火で焼いた。おいしい！")},
		{TEXT("You eat a %s."), TEXT("Você come %s."), TEXT("你吃了%s。"), TEXT("%sを食べた。")},
		{TEXT("Branch"), TEXT("Galho"), TEXT("树枝"), TEXT("枝")},
		{TEXT("Spear"), TEXT("Lança"), TEXT("长矛"), TEXT("槍")},
		{TEXT("Axe"), TEXT("Machado"), TEXT("斧头"), TEXT("斧")},
		{TEXT("Torch"), TEXT("Tocha"), TEXT("火把"), TEXT("たいまつ")},
		{TEXT("Berry seeds"), TEXT("Sementes de baga"), TEXT("浆果种子"), TEXT("ベリーの種")},
		{TEXT("Spores"), TEXT("Esporos"), TEXT("孢子"), TEXT("胞子")},
		{TEXT("Hands"), TEXT("Mãos"), TEXT("空手"), TEXT("素手")},
		{TEXT("[Q] Switch   [Left click] Use   [C] Eat   [J] Journal"), TEXT("[Q] Trocar   [Clique esquerdo] Usar   [C] Comer   [J] Diário"), TEXT("[Q] 切换   [左键] 使用   [C] 吃   [J] 日记"), TEXT("[Q] 持ち替え   [左クリック] 使う   [C] 食べる   [J] 日記")},
		{TEXT("Your torch burns out."), TEXT("Sua tocha se apagou."), TEXT("你的火把熄灭了。"), TEXT("たいまつが燃え尽きた。")},
		{TEXT("Your %s breaks!"), TEXT("Seu %s quebrou!"), TEXT("你的%s坏了！"), TEXT("%sが壊れた！")},
		{TEXT("The creature fears your torch and flees!"), TEXT("A criatura tem medo da sua tocha e foge!"), TEXT("怪物害怕你的火把，逃走了！"), TEXT("怪物はたいまつを恐れて逃げた！")},
		{TEXT("Not here: find free, open ground."), TEXT("Aqui não: procure um chão livre e aberto."), TEXT("这里不行：找一块空旷的地面。"), TEXT("ここは無理：開けた地面を探して。")},
		{TEXT("Planted! Rain makes it grow faster."), TEXT("Plantado! A chuva faz crescer mais rápido."), TEXT("种下了！下雨时长得更快。"), TEXT("植えた！雨が降ると早く育つ。")},
		{TEXT("You planted berry seeds."), TEXT("Você plantou sementes de baga."), TEXT("你种下了浆果种子。"), TEXT("ベリーの種を植えた。")},
		{TEXT("You planted mushroom spores."), TEXT("Você plantou esporos de cogumelo."), TEXT("你种下了蘑菇孢子。"), TEXT("キノコの胞子を植えた。")},
		{TEXT("You drove off %s!"), TEXT("Você espantou %s!"), TEXT("你赶走了%s！"), TEXT("%sを追い払った！")},
		{TEXT("You fought off a creature of the night."), TEXT("Você afastou uma criatura da noite."), TEXT("你击退了一只夜行怪物。"), TEXT("夜の怪物を撃退した。")},
		{TEXT("Hit! It staggers..."), TEXT("Acertou! Ele cambaleia..."), TEXT("命中！它踉跄了一下……"), TEXT("命中！よろめいた……")},
		{TEXT("%s backs off."), TEXT("%s recua."), TEXT("%s退开了。"), TEXT("%sは後ずさりした。")},
		{TEXT("Chop! +2 wood (%d)"), TEXT("Tchac! +2 madeira (%d)"), TEXT("砍！+2 木头（%d）"), TEXT("カン！木+2（%d）")},
		{TEXT("Harvest: +%s. It will grow again."), TEXT("Colheita: +%s. Vai crescer de novo."), TEXT("收获：+%s。它还会再长。"), TEXT("収穫：+%s。また育つ。")},
		{TEXT("You harvested %s from your garden."), TEXT("Você colheu %s da sua horta."), TEXT("你从菜园里收获了%s。"), TEXT("畑から%sを収穫した。")},
		{TEXT("In the chest: %s"), TEXT("No baú: %s"), TEXT("箱子里：%s"), TEXT("宝箱の中：%s")},
		{TEXT("You opened the star's chest: %s."), TEXT("Você abriu o baú da estrela: %s."), TEXT("你打开了星星的箱子：%s。"), TEXT("星の宝箱を開けた：%s。")},
		{TEXT("You found a hidden chest: %s."), TEXT("Você encontrou um baú escondido: %s."), TEXT("你找到了一个隐藏的箱子：%s。"), TEXT("隠された宝箱を見つけた：%s。")},
		{TEXT("a heart fruit (+2 hearts)"), TEXT("uma fruta-coração (+2 corações)"), TEXT("一颗心之果（+2 心）"), TEXT("ハートの実（ハート+2）")},
		{TEXT("a new %s"), TEXT("um novo %s"), TEXT("新的%s"), TEXT("新しい%s")},
		{TEXT("[E]  Open the chest"), TEXT("[E]  Abrir o baú"), TEXT("[E]  打开箱子"), TEXT("[E]  宝箱を開ける")},
		{TEXT("[E]  Harvest"), TEXT("[E]  Colher"), TEXT("[E]  收获"), TEXT("[E]  収穫する")},
		{TEXT("Growing... %d%%"), TEXT("Crescendo... %d%%"), TEXT("生长中……%d%%"), TEXT("成長中……%d%%")},
		{TEXT("[Left click]  Plant (%d left)"), TEXT("[Clique esquerdo]  Plantar (restam %d)"), TEXT("[左键]  种植（剩余%d）"), TEXT("[左クリック]  植える（残り%d）")},
		{TEXT("Day %d of survival"), TEXT("Dia %d de sobrevivência"), TEXT("生存第%d天"), TEXT("サバイバル%d日目")},
		{TEXT("A new day: %s"), TEXT("Um novo dia: %s"), TEXT("新的一天：%s"), TEXT("新しい一日：%s")},
		{TEXT("You survived another night."), TEXT("Você sobreviveu a mais uma noite."), TEXT("你又熬过了一夜。"), TEXT("またひと晩生き延びた。")},
		{TEXT("A new life begins in the forest."), TEXT("Uma nova vida começa na floresta."), TEXT("森林里，新的生活开始了。"), TEXT("森で新しい暮らしが始まる。")},

		// --- Night, survival, seasons -----------------------------------------
		{TEXT("Night falls... Something is moving in the dark. Stay close to a campfire!"), TEXT("A noite cai... Algo se move no escuro. Fique perto de uma fogueira!"), TEXT("夜幕降临……黑暗中有东西在动。待在篝火旁！"), TEXT("夜が来た……暗闇で何かが動いている。たき火のそばにいて！")},
		{TEXT("Dawn! The creatures of the night flee into the trees."), TEXT("Amanhecer! As criaturas da noite fogem para as árvores."), TEXT("黎明！夜行怪物逃进了树林。"), TEXT("夜明けだ！夜の怪物たちが木々の中へ逃げていく。")},
		{TEXT("Red eyes in the dark... %s is hunting you. Run to a fire!"), TEXT("Olhos vermelhos no escuro... %s está caçando você. Corra para uma fogueira!"), TEXT("黑暗中的红眼睛……%s在追猎你。快跑向火堆！"), TEXT("暗闇に赤い目……%sがあなたを狙っている。火のそばへ走って！")},
		{TEXT("Spring"), TEXT("Primavera"), TEXT("春"), TEXT("春")},
		{TEXT("Summer"), TEXT("Verão"), TEXT("夏"), TEXT("夏")},
		{TEXT("Autumn"), TEXT("Outono"), TEXT("秋"), TEXT("秋")},
		{TEXT("Winter"), TEXT("Inverno"), TEXT("冬"), TEXT("冬")},
		{TEXT("Spring is here: the forest turns green again."), TEXT("Chegou a primavera: a floresta volta a ficar verde."), TEXT("春天来了：森林重新变绿。"), TEXT("春が来た：森がまた緑になる。")},
		{TEXT("Summer: long warm days."), TEXT("Verão: dias longos e quentes."), TEXT("夏天：漫长温暖的日子。"), TEXT("夏：長くあたたかな日々。")},
		{TEXT("Autumn: the leaves turn gold and red, the animals fill up before winter."), TEXT("Outono: as folhas ficam douradas e vermelhas, os animais se fartam antes do inverno."), TEXT("秋天：树叶变成金色和红色，动物们在冬天前吃饱。"), TEXT("秋：葉は金色と赤に染まり、動物たちは冬にそなえて食べる。")},
		{TEXT("Winter: snow is coming and food is scarce. Hungry animals are dangerous!"), TEXT("Inverno: a neve chega e a comida fica escassa. Animais famintos são perigosos!"), TEXT("冬天：雪来了，食物稀少。饥饿的动物很危险！"), TEXT("冬：雪が来て、食べ物が少なくなる。おなかをすかせた動物は危険だ！")},
		{TEXT("You fainted...\nThe forest is dangerous when its animals are hungry."), TEXT("Você desmaiou...\nA floresta é perigosa quando os animais estão com fome."), TEXT("你晕倒了……\n动物饥饿时，森林很危险。"), TEXT("あなたは気を失った……\n動物が飢えているとき、森は危険だ。")},
		{TEXT("%s is hungry and growls... Feed it or step back!"), TEXT("%s está com fome e rosna... Alimente-o ou se afaste!"), TEXT("%s饿了，正在低吼……喂它或者后退！"), TEXT("%sはおなかをすかせてうなっている……えさをあげるか、下がって！")},
		{TEXT("Ouch! %s bit you. Give it some food!"), TEXT("Ai! %s mordeu você. Dê comida a ele!"), TEXT("哎哟！%s咬了你。给它点吃的！"), TEXT("痛っ！%sにかまれた。何か食べさせて！")},
		{TEXT("You wake up by your campfire, rested."), TEXT("Você acorda ao lado da sua fogueira, descansada."), TEXT("你在篝火旁醒来，休息好了。"), TEXT("たき火のそばで目を覚ました。疲れがとれた。")},
		{TEXT("You wake up where your journey began. Build a campfire to wake up there."), TEXT("Você acorda onde sua jornada começou. Construa uma fogueira para acordar lá."), TEXT("你在旅程开始的地方醒来。建一堆篝火，就能在那里醒来。"), TEXT("旅が始まった場所で目を覚ました。たき火を作れば、そこで目覚められる。")},
	};

	const TMap<FString, const FWorldTranslation*>& WorldIndex()
	{
		static TMap<FString, const FWorldTranslation*> Map;
		if (Map.IsEmpty())
		{
			for (const FWorldTranslation& Entry : GWorldTable)
			{
				Map.Add(Entry.English, &Entry);
			}
		}
		return Map;
	}
}

/** Used by ForestLanguage::Translate for Portuguese, Chinese and Japanese. */
const TCHAR* ForestTranslateWorld(const TCHAR* English, EForestLanguage Language)
{
	const FWorldTranslation* const* Found = English ? WorldIndex().Find(English) : nullptr;
	if (!Found)
	{
		return nullptr;
	}
	switch (Language)
	{
	case EForestLanguage::Portuguese: return (*Found)->Portuguese;
	case EForestLanguage::Chinese: return (*Found)->Chinese;
	case EForestLanguage::Japanese: return (*Found)->Japanese;
	default: return nullptr;
	}
}
