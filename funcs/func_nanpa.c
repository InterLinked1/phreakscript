/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2022, Naveen Albert
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief NANPA Number Function
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 * \ingroup functions
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/pbx.h"
#include "asterisk/channel.h"
#include "asterisk/utils.h"

/*** DOCUMENTATION
	<function name="NANPA" language="en_US">
		<synopsis>
			Retrieve information about a North American Numbering Plan area.
		</synopsis>
		<syntax>
			<parameter name="npa" required="true">
				<para>Numbering Plan Area (i.e. area code)</para>
			</parameter>
			<parameter name="property" required="true">
				<para>Must be one of the following:</para>
				<enumlist>
					<enum name="base">
						<para>The base NPA.</para>
						<para>This is the original NPA in overlaid areas with more than one NPA.</para>
					</enum>
					<enum name="overlaid">
						<para>Whether or not the area code is overlaid.</para>
					</enum>
					<enum name="zip">
						<para>ZIP code corresponding to the area code.</para>
						<para>Note that the ZIP code returned is, while in the NPA, somewhat arbitrary,
						though often in a prominent city in the NPA.</para>
					</enum>
					<enum name="cityid">
						<para>OpenWeatherMap City ID of a prominent city in this NPA.</para>
					</enum>
					<enum name="tz">
						<para>TZ format time zone of this NPA.</para>
					</enum>
					<enum name="city">
						<para>Filename (for use with Playback) of prominent city in this NPA.</para>
					</enum>
					<enum name="state">
						<para>Filename (for use with Playback) of NPA's state/region.</para>
					</enum>
				</enumlist>
			</parameter>
		</syntax>
		<description>
			<para>This function will return information about a Numbering Plan Area (area code).</para>
			<para>By default, it will return the base (original) area code of the specified NPA.</para>
		</description>
	</function>
 ***/

/*! \note
 * Yes, I am perfectly aware this is stretching the limits of
 * things for which it may make sense to write an Asterisk module...
 * It's purely data lookup, and I make no pretense of hiding that.
 * This is generic enough that I thought it was worth doing,
 * and it made sense for my purposes... it's purely a convenience function.
 * Go use something else if you don't like it.
 * DISCLAIMER: This is not updated regularly and no guarantee is made to its accuracy.
 */

static int npa_overlaid(int npa)
{
	switch (npa) {
	case 201:
	case 203:
	case 204:
	case 205:
	case 208:
	case 210:
	case 212:
	case 213:
	case 214:
	case 215:
	case 217:
	case 220:
	case 223:
	case 224:
	case 226:
	case 227:
	case 234:
	case 236:
	case 240:
	case 248:
	case 249:
	case 256:
	case 263:
	case 267:
	case 270:
	case 272:
	case 279:
	case 281:
	case 289:
	case 301:
	case 303:
	case 304:
	case 305:
	case 306:
	case 310:
	case 312:
	case 315:
	case 317:
	case 321:
	case 323:
	case 326:
	case 330:
	case 331:
	case 332:
	case 336:
	case 339:
	case 341:
	case 343:
	case 346:
	case 347:
	case 351:
	case 354:
	case 360:
	case 364:
	case 365:
	case 367:
	case 368:
	case 380:
	case 385:
	case 402:
	case 407:
	case 408:
	case 410:
	case 412:
	case 416:
	case 418:
	case 419:
	case 424:
	case 430:
	case 431:
	case 437:
	case 438:
	case 442:
	case 443:
	case 445:
	case 447:
	case 448:
	case 450:
	case 458:
	case 463:
	case 464:
	case 468:
	case 470:
	case 474:
	case 475:
	case 503:
	case 508:
	case 510:
	case 512:
	case 514:
	case 518:
	case 519:
	case 531:
	case 534:
	case 539:
	case 541:
	case 548:
	case 551:
	case 564:
	case 567:
	case 569:
	case 570:
	case 571:
	case 579:
	case 581:
	case 582:
	case 587:
	case 601:
	case 609:
	case 610:
	case 613:
	case 614:
	case 615:
	case 617:
	case 619:
	case 629:
	case 630:
	case 631:
	case 639:
	case 640:
	case 646:
	case 647:
	case 657:
	case 659:
	case 667:
	case 669:
	case 672:
	case 678:
	case 680:
	case 681:
	case 682:
	case 702:
	case 703:
	case 704:
	case 705:
	case 706:
	case 713:
	case 714:
	case 715:
	case 717:
	case 718:
	case 720:
	case 724:
	case 725:
	case 726:
	case 732:
	case 737:
	case 740:
	case 742:
	case 743:
	case 747:
	case 753:
	case 754:
	case 760:
	case 762:
	case 769:
	case 773:
	case 774:
	case 778:
	case 779:
	case 781:
	case 782:
	case 786:
	case 787:
	case 801:
	case 803:
	case 805:
	case 812:
	case 814:
	case 815:
	case 817:
	case 818:
	case 819:
	case 820:
	case 825:
	case 832:
	case 838:
	case 839:
	case 840:
	case 843:
	case 847:
	case 848:
	case 850:
	case 854:
	case 857:
	case 858:
	case 860:
	case 862:
	case 872:
	case 873:
	case 878:
	case 902:
	case 903:
	case 905:
	case 909:
	case 916:
	case 917:
	case 918:
	case 919:
	case 929:
	case 930:
	case 934:
	case 937:
	case 938:
	case 939:
	case 943:
	case 945:
	case 947:
	case 954:
	case 959:
	case 971:
	case 972:
	case 973:
	case 978:
	case 980:
	case 984:
	case 986:
		return 1;
	default:
		/* This is guaranteed to be valid because we found the NPA in the npa_base lookup first. */
		return 0;
	}
}

static int npa_base(int npa)
{
	switch (npa) {
	/* Thank goodness for this cool range trick (it's a gcc extension but we can use it since that's what Asterisk uses): */
	case 201 ... 210:
	case 212 ... 219:
		return npa;
	case 220: return 740;
	case 223: return 717;
	case 224: return 847;
	case 225:
	case 228:
	case 229:
		return npa;
	case 226: return 519;
	case 227: return 301;
	case 231:
	case 239:
		return npa;
	case 234: return 330;
	case 236: return 250;
	case 240: return 301;
	case 242: return 809;
	case 246:
	case 248:
		return npa;
	case 249: return 705;
	case 250 ... 254:
	case 256:
		return npa;
	case 260:
	case 262:
	case 264:
	case 268:
	case 269:
		return npa;
	case 263: return 514;
	case 267: return 215;
	case 270:
	case 276:
		return npa;
	case 272: return 570;
	case 274: return 920;
	case 279: return 916;
	case 281: return 713;
	case 283: return 513;
	case 284: return npa;
	case 289: return 905;
	case 301 ... 310:
	case 312 ... 321:
	case 325:
		return npa;
	case 323: return 213;
	case 326: return 937;
	case 327: return 870;
	case 331: return 630;
	case 332: return 212;
	case 334:
	case 336:
	case 337:
		return npa;
	case 339: return 781;
	case 340:
	case 345:
		return npa;
	case 341: return 510;
	case 343: return 613;
	case 346: return 713;
	case 347: return 718;
	case 351: return 978;
	case 354: return 450;
	case 352: return npa;
	case 360:
	case 361:
		return npa;
	case 364: return 270;
	case 365: return 905;
	case 367: return 418;
	case 368: return 587;
	case 380: return 614;
	case 382: return 519;
	case 385: return 801;
	case 386: return npa;
	case 387: return 416;
	case 401 ... 410:
	case 412 ... 419:
		return npa;
	case 423:
	case 425:
		return npa;
	case 424: return 310;
	case 428: return 506;
	case 430: return 903;
	case 431: return 204;
	case 432:
	case 434:
	case 435:
		return npa;
	case 437: return 416;
	case 438: return 514;
	case 440:
	case 441:
		return npa;
	case 442: return 760;
	case 443: return 410;
	case 445: return 215;
	case 447: return 217;
	case 448: return 850;
	case 450:
		return npa;
	case 458: return 541;
	case 463: return 317;
	case 464: return 708;
	case 468: return 819;
	case 469: return 214;
	case 470: return 404;
	case 473:
	case 478:
	case 479:
		return npa;
	case 474: return 306;
	case 475: return 203;
	case 480: return npa;
	case 484: return 610;
	case 501 ... 510:
	case 512 ... 520:
		return npa;
	case 530: return npa;
	case 531: return 402;
	case 534: return 715;
	case 539: return 918;
	case 540:
	case 541:
		return npa;
	case 548: return 519;
	case 551: return 201;
	case 557: return 314;
	case 559: return npa;
	case 561 ... 563:
		return npa;
	case 564: return 360;
	case 567: return 419;
	case 570:
	case 573 ... 575:
		return npa;
	case 571: return 703;
	case 572: return 405;
	case 579: return 450;
	case 580:
	case 585 ... 587:
		return npa;
	case 581: return 418;
	case 582: return 814;
	case 584: return 204;
	case 601 ... 610:
	case 612 ... 620:
	case 623:
	case 626:
		return npa;
	case 628: return 415;
	case 629: return 615;
	case 630:
	case 631:
	case 636:
		return npa;
	case 639: return 306;
	case 640: return 609;
	case 641:
	case 649:
		return npa;
	case 646: return 212;
	case 647: return 416;
	case 650:
	case 651:
		return npa;
	case 657: return 714;
	case 658: return 876;
	case 659: return 659;
	case 660:
	case 661:
	case 662:
	case 664:
		return npa;
	case 667: return 410;
	case 669: return 408;
	case 670:
	case 671:
		return npa;
	case 672: return 250;
	case 678: return 404;
	case 679: return 313;
	case 680: return 315;
	case 681: return 304;
	case 682: return 817;
	case 683: return 705;
	case 684: return npa;
	case 689: return 407;
	case 701 ... 709:
	case 712 ... 719:
		return npa;
	case 720: return 303;
	case 721:
	case 724:
	case 727:
		return npa;
	case 725: return 702;
	case 726: return 210;
	case 730: return 618;
	case 731:
	case 732:
	case 734:
		return npa;
	case 737: return 512;
	case 740: return npa;
	case 742: return 905;
	case 743: return 336;
	case 747: return 818;
	case 753: return 613;
	case 754: return 954;
	case 757:
	case 758:
		return npa;
	case 760:
	case 763:
	case 765:
	case 767:
		return npa;
	case 762: return 706;
	case 764: return 650;
	case 769: return 601;
	case 770:
	case 772:
	case 773:
	case 775:
		return npa;
	case 774: return 508;
	case 778: return 250;
	case 779: return 815;
	case 780: return 403;
	case 781:
	case 784:
	case 785:
	case 787:
		return npa;
	case 782: return 902;
	case 786: return 305;
	case 801 ... 810:
	case 812 ... 819:
		return npa;
	case 820: return 805;
	case 825: return 587;
	case 828: return npa;
	case 829: return 809;
	case 830:
	case 831:
		return npa;
	case 832: return 713;
	case 838: return 518;
	case 839: return 803;
	case 840: return 909;
	case 843:
	case 845:
	case 847:
		return npa;
	case 848: return 732;
	case 849: return 809;
	case 850:
	case 856:
	case 859:
		return npa;
	case 854: return 843;
	case 857: return 617;
	case 858: return 619;
	case 860:
	case 863 ... 865:
	case 867 ... 869:
		return npa;
	case 862: return 973;
	case 870:
	case 876:
		return npa;
	case 872: return 312;
	case 873: return 819;
	case 878: return 412;
	case 901 ... 910:
	case 912 ... 916:
	case 918 ... 919:
		return npa;
	case 917: return 212;
	case 920:
	case 925:
	case 927:
	case 928:
		return npa;
	case 929: return 347;
	case 930: return 812;
	case 931:
	case 936:
	case 937:
		return npa;
	case 934: return 631;
	case 938: return 256;
	case 939: return 787;
	case 940:
	case 941:
	case 949:
		return npa;
	case 942: return 416;
	case 947: return 248;
	case 951:
	case 952:
	case 954:
	case 956:
		return npa;
	case 959: return 860;
	case 970:
	case 973:
	case 978:
	case 979:
		return npa;
	case 971: return 503;
	case 972: return 214;
	case 980: return 704;
	case 984: return 919;
	case 985:
	case 989:
		return npa;
	case 986: return 208;
	default:
		return -1;
	}
}

static const char *npa_zip(int base)
{
	switch (base) {
	case 201: return "07302";
	case 202: return "20500";
	case 203: return "06510";
	case 204: return "00000";
	case 205: return "35203";
	case 206: return "98101";
	case 207: return "04101";
	case 208: return "83704";
	case 209: return "95202";
	case 210: return "78205";
	case 212: return "10010";
	case 213: return "90011";
	case 214: return "75202";
	case 215: return "19102";
	case 216: return "44114";
	case 217: return "97475";
	case 218: return "55802";
	case 219: return "46402";
	case 225: return "70801";
	case 228: return "39581";
	case 229: return "12210";
	case 231: return "49685";
	case 239: return "33907";
	case 246: return "00000";
	case 248: return "48306";
	case 250: return "00000";
	case 251: return "36603";
	case 252: return "29601";
	case 253: return "98405";
	case 254: return "76701";
	case 256: return "35816";
	case 260: return "46802";
	case 262: return "53186";
	case 264: return "00000";
	case 268: return "00000";
	case 269: return "49037";
	case 270: return "42103";
	case 276: return "24202";
	case 284: return "00000";
	case 301: return "20901";
	case 302: return "19802";
	case 303: return "80203";
	case 304: return "25309";
	case 305: return "33101";
	case 306: return "00000";
	case 307: return "82006";
	case 308: return "68801";
	case 309: return "85345";
	case 310: return "90212";
	case 312: return "60601";
	case 313: return "48201";
	case 314: return "63108";
	case 315: return "13212";
	case 316: return "67202";
	case 317: return "46202";
	case 318: return "71109";
	case 319: return "52401";
	case 320: return "56396";
	case 321: return "32920";
	case 325: return "79605";
	case 334: return "36107";
	case 336: return "27403";
	case 337: return "70501";
	case 340: return "00000";
	case 345: return "00000";
	case 352: return "32601";
	case 360: return "98663";
	case 361: return "78405";
	case 386: return "32114";
	case 401: return "02903";
	case 402: return "68046";
	case 403: return "00000";
	case 404: return "30303";
	case 405: return "73103";
	case 406: return "59715";
	case 407: return "32801";
	case 408: return "95112";
	case 409: return "77551";
	case 410: return "21211";
	case 412: return "15211";
	case 413: return "01301";
	case 414: return "53202";
	case 415: return "94103";
	case 416: return "00000";
	case 417: return "65806";
	case 418: return "00000";
	case 419: return "43604";
	case 423: return "37401";
	case 425: return "98005";
	case 432: return "79701";
	case 434: return "22904";
	case 435: return "84111";
	case 440: return "44052";
	case 441: return "00000";
	case 450: return "00000";
	case 473: return "00000";
	case 478: return "31201";
	case 479: return "28301";
	case 480: return "85258";
	case 501: return "72204";
	case 502: return "40204";
	case 503: return "97298";
	case 504: return "70112";
	case 505: return "87501";
	case 506: return "00000";
	case 507: return "55905";
	case 508: return "02636";
	case 509: return "99202";
	case 510: return "94612";
	case 512: return "78701";
	case 513: return "45206";
	case 514: return "00000";
	case 515: return "50309";
	case 516: return "11550";
	case 517: return "48910";
	case 518: return "12206";
	case 519: return "00000";
	case 520: return "85701";
	case 530: return "96122";
	case 540: return "22401";
	case 541: return "97401";
	case 559: return "93705";
	case 561: return "33480";
	case 562: return "90806";
	case 563: return "52803";
	case 570: return "18503";
	case 573: return "65109";
	case 574: return "46514";
	case 575: return "88001";
	case 580: return "74601";
	case 585: return "14603";
	case 586: return "48021";
	case 587: return "00000";
	case 601: return "39202";
	case 602: return "85004";
	case 603: return "03301";
	case 604: return "00000";
	case 605: return "57501";
	case 606: return "41101";
	case 607: return "14850";
	case 608: return "53702";
	case 609: return "68404";
	case 610: return "19013";
	case 612: return "55402";
	case 613: return "00000";
	case 614: return "43201";
	case 615: return "37115";
	case 616: return "49503";
	case 617: return "02114";
	case 618: return "62901";
	case 619: return "92110";
	case 620: return "67005";
	case 623: return "85301";
	case 626: return "91775";
	case 630: return "60101";
	case 631: return "11717";
	case 636: return "63303";
	case 641: return "50401";
	case 649: return "00000";
	case 650: return "94301";
	case 651: return "55103";
	case 658: return "00000";
	case 660: return "65301";
	case 661: return "93301";
	case 662: return "38804";
	case 664: return "00000";
	case 670: return "00000";
	case 671: return "00000";
	case 684: return "00000";
	case 701: return "58103";
	case 702: return "89102";
	case 703: return "22203";
	case 704: return "28202";
	case 705: return "00000";
	case 706: return "30904";
	case 707: return "95501";
	case 708: return "60302";
	case 709: return "00000";
	case 712: return "51103";
	case 713: return "77002";
	case 714: return "92805";
	case 715: return "54703";
	case 716: return "14201";
	case 717: return "17103";
	case 718: return "11201";
	case 719: return "80903";
	case 721: return "00000";
	case 724: return "15601";
	case 727: return "33706";
	case 731: return "38024";
	case 732: return "08901";
	case 734: return "48104";
	case 740: return "45701";
	case 757: return "23662";
	case 758: return "00000";
	case 760: return "92262";
	case 763: return "55445";
	case 765: return "94549";
	case 767: return "00000";
	case 770: return "30120";
	case 772: return "34950";
	case 773: return "60612";
	case 775: return "90745";
	case 781: return "02062";
	case 784: return "00000";
	case 785: return "66604";
	case 787: return "00000";
	case 801: return "84180";
	case 802: return "05602";
	case 803: return "29208";
	case 804: return "23221";
	case 805: return "93003";
	case 806: return "79410";
	case 807: return "00000";
	case 808: return "96813";
	case 809: return "00000";
	case 810: return "48060";
	case 812: return "47408";
	case 813: return "33602";
	case 814: return "16501";
	case 815: return "61104";
	case 816: return "64102";
	case 817: return "76102";
	case 818: return "91502";
	case 819: return "00000";
	case 828: return "28801";
	case 830: return "78840";
	case 831: return "95062";
	case 843: return "29407";
	case 845: return "12601";
	case 847: return "61937";
	case 850: return "32306";
	case 856: return "08002";
	case 859: return "40504";
	case 860: return "06103";
	case 863: return "33801";
	case 864: return "29601";
	case 865: return "37916";
	case 867: return "00000";
	case 868: return "00000";
	case 869: return "00000";
	case 870: return "72401";
	case 876: return "00000";
	case 901: return "38104";
	case 902: return "00000";
	case 903: return "75701";
	case 904: return "32202";
	case 905: return "00000";
	case 906: return "49783";
	case 907: return "99503";
	case 908: return "07202";
	case 909: return "92335";
	case 910: return "28301";
	case 912: return "31404";
	case 913: return "66102";
	case 914: return "10461";
	case 915: return "79903";
	case 916: return "94203";
	case 918: return "74103";
	case 919: return "27608";
	case 920: return "54911";
	case 925: return "94551";
	case 927: return "34741";
	case 928: return "86001";
	case 931: return "37042";
	case 936: return "75964";
	case 937: return "45400";
	case 940: return "76201";
	case 941: return "34239";
	case 949: return "92614";
	case 951: return "92506";
	case 952: return "55420";
	case 954: return "33311";
	case 956: return "78040";
	case 970: return "81501";
	case 973: return "07102";
	case 978: return "01420";
	case 979: return "77488";
	case 985: return "70360";
	case 989: return "49707";
	default:
		ast_assert_return(NULL, 0);
	}
}

static const char *npa_locale(int base)
{
	switch (base) {
	case 201: return "5097672,America/New_York,englewood,new-jersey";
	case 202: return "4140963,America/New_York,washington-dc";
	case 203: return "5282804,America/New_York,bridgeport,connecticut";
	case 204: return "6183235,America/Chicago,winnipeg,manitoba";
	case 205: return "4049979,America/Chicago,birmingham,alabama";
	case 206: return "5809844,America/Los_Angeles,seattle,washington";
	case 207: return "4975802,America/New_York,portland,maine";
	case 208: return "5586437,America/Denver,boise,idaho";
	case 209: return "5399020,America/Los_Angeles,stockton,california";
	case 210: return "4726206,America/Chicago,san-antonio,texas";
	case 212: return "5125771,America/New_York,new-york,new-york";
	case 213: return "5368361,America/Los_Angeles,los-angeles,california";
	case 214: return "4190598,America/Chicago,dallas,texas";
	case 215: return "4560349,America/New_York,philadelphia,pennsylvania";
	case 216: return "5150529,America/New_York,cleveland,ohio";
	case 217: return "4250542,America/Chicago,springfield,illinois";
	case 218: return "5024719,America/Chicago,duluth,minnesota";
	case 219: return "4920607,America/Chicago,gary,indiana";
	case 220: return "5164466,America/New_York,newark,ohio";
	case 223: return "5197079,America/New_York,lancaster,pennsylvania";
	case 224: return "4890864,America/Chicago,elgin,illinois";
	case 225: return "4315588,America/Chicago,batonrouge,louisiana";
	case 226: return "6182962,America/New_York,windsor,ontario";
	case 227: return "4369596,America/New_York,silverspring,maryland";
	case 228: return "4418478,America/Chicago,biloxi,mississippi";
	case 229: return "4179320,America/New_York,albany,georgia";
	case 231: return "5003132,America/New_York,muskegon,michigan";
	case 234: return "5145476,America/New_York,akron,ohio";
	case 236: return "6173331,America/Los_Angeles,vancouver,britishcolumbia";
	case 239: return "4149962,America/New_York,capecoral,florida";
	case 240: return "4348599,America/New_York,bethesda,maryland";
	case 242: return "3571824,America/New_York,nassau,bahamas";
	case 246: return "3374036,America/Barbados,bridgetown,barbados";
	case 248: return "5006166,America/New_York,pontiac,michigan";
	case 249: return "5009004,America/New_York,saultstemarie,michigan";
	case 250: return "6174041,America/Los_Angeles,victoria,britishcolumbia";
	case 251: return "4076598,America/Chicago,mobile,alabama";
	case 252: return "4488762,America/New_York,rockymount,northcarolina";
	case 253: return "5812944,America/Los_Angeles,tacoma,washington";
	case 254: return "4739526,America/Chicago,waco,texas";
	case 256: return "4068590,America/Chicago,huntsville,alabama";
	case 260: return "4920423,America/New_York,fortwayne,indiana";
	case 262: return "5278052,America/Chicago,waukesha,wisconsin";
	case 263: return "6077243,America/New_York,montreal,quebec";
	case 264: return "3573374,America/Barbados,thevalley,anguilla";
	case 267: return "4560349,America/New_York,philadelphia,pennsylvania";
	case 268: return "3576022,America/Barbados,stjohns,antiguaandbarbuda";
	case 269: return "4985153,America/New_York,battlecreek,michigan";
	case 270: return "5147968,America/Chicago,bowlinggreen,kentucky";
	case 272: return "5211303,America/New_York,scranton,pennsylvania";
	case 274: return "5254962,America/Chicago,greenbay,wisconsin";
	case 276: return "4749005,America/New_York,bristol,virginia";
	case 279: return "5389489,America/Los_Angeles,sacramento,california";
	case 281: return "4699066,America/Chicago,houston,texas";
	case 283: return "4508722,America/New_York,cincinnati,ohio";
	case 284: return "3577430,America/Barbados,roadtown,britishvirginislands";
	case 289: return "5969782,America/New_York,hamilton,ontario";
	case 301: return "4369596,America/New_York,silverspring,maryland";
	case 302: return "4145381,America/New_York,wilmington,delaware";
	case 303: return "5419384,America/Denver,denver,colorado";
	case 304: return "4822764,America/New_York,charleston,west-virginia";
	case 305: return "4164138,America/New_York,miami,florida";
	case 306: return "6141256,America/Chicago,saskatoon,saskatchewan";
	case 307: return "5821086,America/Denver,cheyenne,wyoming";
	case 308: return "4393269,America/Chicago,kearney,nebraska";
	case 309: return "4905687,America/Chicago,peoria,illinois";
	case 310: return "5369906,America/Los_Angeles,malibu,california";
	case 312: return "4887398,America/Chicago,chicago,illinois";
	case 313: return "4990510,America/New_York,dearborn,michigan";
	case 314: return "4407084,America/Chicago,stlouis,missouri";
	case 315: return "5140405,America/New_York,syracuse,new-york";
	case 316: return "4281730,America/Chicago,wichita,kansas";
	case 317: return "4259418,America/New_York,indianapolis,indiana";
	case 318: return "4341513,America/Chicago,shreveport,louisiana";
	case 319: return "4850751,America/Chicago,cedarrapids,iowa";
	case 320: return "5044407,America/Chicago,stcloud,minnesota";
	case 321: return "4167147,America/New_York,orlando,florida";
	case 323: return "5368361,America/Los_Angeles,losangeles,california";
	case 325: return "4669635,America/Chicago,abilene,texas";
	case 326: return "4289629,America/New_York,dayton,ohio";
	case 327: return "4116834,America/Chicago,jonesboro,arkansas";
	case 330: return "5145476,America/New_York,akron,ohio";
	case 331: return "4883817,America/Chicago,aurora,illinois";
	case 332: return "5125771,America/New_York,manhattan,new-york";
	case 334: return "4076784,America/Chicago,montgomery,alabama";
	case 336: return "4469146,America/New_York,greensboro,northcarolina";
	case 337: return "4330145,America/Chicago,lafayette,louisiana";
	case 339: return "4943097,America/New_York,marblehead,massachusetts";
	case 340: return "4795467,America/Barbados,charlotteamalie,usvirginislands";
	case 341: return "5378538,America/Los_Angeles,oakland,california";
	case 343: return "6094817,America/New_York,ottawa,ontario";
	case 345: return "3580718,America/New_York,cayman,islands";
	case 346: return "4699066,America/Chicago,houston,texas";
	case 347: return "5115985,America/New_York,queens,new-york";
	case 351: return "4942618,America/New_York,lowell,ma";
	case 352: return "4156404,America/New_York,gainesville,fl";
	case 360: return "5805687,America/Los_Angeles,olympia,washington";
	case 361: return "4683416,America/Chicago,corpuschristi,texas";
	case 364: return "4303436,America/Chicago,owensboro,kentucky";
	case 365: return "5969782,America/New_York,hamilton,ontario";
	case 367: return "6325494,America/New_York,quebeccity,quebec";
	case 368: return "5946768,America/Denver,edmonton,alberta";
	case 380: return "4509177,America/New_York,columbus,ohio";
	case 382: return "6058560,America/New_York,london,ontario";
	case 385: return "5780993,America/Denver,saltlakecity,utah";
	case 386: return "4152872,America/New_York,daytonabeach,florida";
	case 387: return "6167863,America/New_York,toronto,ontario";
	case 401: return "5224162,America/New_York,providence,rhode-island";
	case 402: return "5074472,America/Chicago,omaha,nebraska";
	case 403: return "5913490,America/Denver,calgary,alberta";
	case 404: return "4180439,America/New_York,atlanta,georgia";
	case 405: return "4544349,America/Chicago,oklahoma-city,oklahoma";
	case 406: return "5641727,America/Denver,bozeman,montana";
	case 407: return "4160983,America/New_York,kissimmee,florida";
	case 408: return "5392171,America/Los_Angeles,san-jose,california";
	case 409: return "4692883,America/Chicago,galveston,texas";
	case 410: return "4347820,America/New_York,baltimore,maryland";
	case 412: return "5206379,America/New_York,pittsburgh,pennsylvania";
	case 413: return "4951788,America/New_York,springfield,massachusetts";
	case 414: return "5263045,America/Chicago,milwaukee,wisconsin";
	case 415: return "5391959,America/Los_Angeles,san-francisco,california";
	case 416: return "6167863,America/New_York,toronto,ontario";
	case 417: return "4409896,America/Chicago,springfield,missouri";
	case 418: return "6325494,America/New_York,quebeccity,quebec";
	case 419: return "5174035,America/New_York,toledo,ohio";
	case 423: return "4612862,America/New_York,chattanooga,tennessee";
	case 424: return "5393212,America/Los_Angeles,santamonica,california";
	case 425: return "5808079,America/Los_Angeles,redmond,washington";
	case 428: return "6076211,America/Los_Angeles,moncton,newbrunswick";
	case 430: return "4738214,America/Chicago,tyler,texas";
	case 431: return "6183235,America/Chicago,winnipeg,manitoba";
	case 432: return "5526337,America/Chicago,midland,texas";
	case 434: return "4752031,America/New_York,charlottesville,northcarolina";
	case 435: return "5546220,America/Denver,stgeorge,utah";
	case 437: return "6167863,America/New_York,toronto,ontario";
	case 438: return "6077243,America/New_York,montreal,quebec";
	case 440: return "5161262,America/New_York,lorain,ohio";
	case 441: return "3573198,America/Barbados,hamilton,bermuda";
	case 442: return "5328808,America/Los_Angeles,bishop,california";
	case 443: return "4347820,America/New_York,baltimore,maryland";
	case 445: return "4440906,America/New_York,philadelphia,pennsylvania";
	case 447: return "4250542,America/Chicago,springfield,illinois";
	case 450: return "6059891,America/New_York,longueuil,quebec";
	case 458: return "5725846,America/Los_Angeles,eugene,oregon";
	case 463: return "4259418,America/New_York,indianapolis,indiana";
	case 469: return "4190598,America/Chicago,dallas,texas";
	case 470: return "4180439,America/New_York,atlanta,georgia";
	case 473: return "3579925,America/Barbados,stgeorges,grenada";
	case 474: return "6141256,America/Chicago,saskatoon,saskatchewan";
	case 475: return "5282804,America/New_York,bridgeport,ct";
	case 478: return "4207400,America/New_York,macon,georgia";
	case 479: return "4110486,America/Chicago,fayetteville,arkansas";
	case 480: return "5313457,America/Phoenix,scottsdale,arizona";
	case 484: return "5180225,America/New_York,bethlehem,pennsylvania";
	case 501: return "4119403,America/Chicago,littlerock,arkansas";
	case 502: return "4299276,America/New_York,louisville,kentucky";
	case 503: return "5746545,America/Los_Angeles,portland,oregon";
	case 504: return "4335045,America/Chicago,new-orleans,louisiana";
	case 505: return "5454711,America/Denver,albuquerque,new-mexico";
	case 506: return "6076211,America/Los_Angeles,moncton,newbrunswick";
	case 507: return "5053156,America/Chicago,winona,minnesota";
	case 508: return "4954944,America/New_York,capecod,massachusetts";
	case 509: return "5809844,America/Los_Angeles,seattle,washington";
	case 510: return "5378538,America/Los_Angeles,oakland,california";
	case 512: return "4671654,America/Chicago,austin,texas";
	case 513: return "4508722,America/New_York,cincinnati,ohio";
	case 514: return "6077243,America/New_York,montreal,quebec";
	case 515: return "4853828,America/Chicago,desmoines,iowa";
	case 516: return "5120478,America/New_York,hempstead,new-york";
	case 517: return "4998830,America/New_York,lansing,michigan";
	case 518: return "5106834,America/New_York,albany,new-york";
	case 519: return "6058560,America/New_York,london,ontario";
	case 520: return "5318313,America/Phoenix,tucson,arizona";
	case 530: return "5384438,America/Los_Angeles,portola,california";
	case 531: return "5074472,America/Chicago,omaha,nebraska";
	case 534: return "5268720,America/Chicago,rhinelander,wisconsin";
	case 539: return "4553433,America/Chicago,tulsa,oklahoma";
	case 540: return "4782241,America/New_York,roanoke,virginia";
	case 541: return "5725846,America/Los_Angeles,eugene,oregon";
	case 548: return "6058560,America/New_York,london,ontario";
	case 551: return "5097672,America/New_York,englewood,new-jersey";
	case 557: return "4407084,America/Chicago,stlouis,missouri";
	case 559: return "5350937,America/Los_Angeles,fresno,california";
	case 561: return "4167505,America/New_York,palmbeach,florida";
	case 562: return "5367929,America/Los_Angeles,longbeach,california";
	case 563: return "4853423,America/Chicago,davenport,iowa";
	case 564: return "5805687,America/Los_Angeles,olympia,washington";
	case 567: return "5174035,America/New_York,toledo,ohio";
	case 570: return "5211303,America/New_York,scranton,pennsylvania";
	case 571: return "4744091,America/New_York,alexandria,virginia";
	case 573: return "4392388,America/Chicago,jeffersoncity,missouri";
	case 574: return "4919987,America/New_York,elkhart,indiana";
	case 575: return "5475352,America/Denver,lascruces,new-mexico";
	case 579: return "6059891,America/New_York,longueuil,quebec";
	case 580: return "4548267,America/Chicago,poncacity,oklahoma";
	case 581: return "6325494,America/New_York,quebeccity,quebec";
	case 584: return "6183235,America/Chicago,winnipeg,manitoba";
	case 585: return "5134086,America/New_York,rochester,new-york";
	case 586: return "5014051,America/New_York,warren,michigan";
	case 587: return "5913490,America/Denver,calgary,alberta";
	case 601: return "4431410,America/Chicago,jackson,mississippi";
	case 602: return "5308655,America/Phoenix,phoenix,arizona";
	case 603: return "5084868,America/New_York,concord,new-hampshire";
	case 604: return "6173331,America/Los_Angeles,vancouver,britishcolumbia";
	case 605: return "5767918,America/Chicago,pierre,south-dakota";
	case 606: return "4282757,America/New_York,ashland,kentucky";
	case 607: return "4997346,America/New_York,ithaca,new-york";
	case 608: return "5261457,America/Chicago,madison,wisconsin";
	case 609: return "5102922,America/New_York,princeton,new-jersey";
	case 610: return "5180225,America/New_York,bethlehem,pennsylvania";
	case 612: return "5037649,America/Chicago,minneapolis,minnesota";
	case 613: return "6094817,America/New_York,ottawa,ontario";
	case 614: return "4509177,America/New_York,columbus,ohio";
	case 615: return "4644585,America/Chicago,nashville,tennessee";
	case 616: return "4994358,America/New_York,grandrapids,michigan";
	case 617: return "4930956,America/New_York,boston,massachusetts";
	case 618: return "4234969,America/Chicago,cahokia,illinois";
	case 619: return "5391811,America/Los_Angeles,san-diego,california";
	case 620: return "5445298,America/Chicago,dodgecity,kansas";
	case 623: return "5295985,America/Phoenix,glendale,arizona";
	case 626: return "5381396,America/Los_Angeles,pasadena,california";
	case 628: return "5391959,America/Los_Angeles,san-francisco,california";
	case 629: return "4644585,America/Chicago,nashville,tennessee";
	case 630: return "4903279,America/Chicago,naperville,illinois";
	case 631: return "5127561,America/New_York,morrisville,new-york";
	case 636: return "4412454,America/Chicago,union,missouri";
	case 639: return "6141256,America/Chicago,saskatoon,saskatchewan";
	case 640: return "5102922,America/New_York,princeton,new-jersey";
	case 641: return "4866445,America/Chicago,masoncity,iowa";
	case 646: return "5125771,America/New_York,manhattan,new-york";
	case 647: return "6167863,America/New_York,toronto,ontario";
	case 649: return "3576994,America/New_York,cockburntown,caicos";
	case 650: return "5380748,America/Los_Angeles,paloalto,california";
	case 651: return "5037280,America/Chicago,mendotaheights,minnesota";
	case 657: return "5323810,America/Los_Angeles,anaheim,california";
	case 658: return "3489854,America/New_York,kingston,jamaica";
	case 659: return "4049979,America/Chicago,birmingham,alabama";
	case 660: return "4408000,America/Chicago,sedalia,missouri";
	case 661: return "5325738,America/Los_Angeles,bakersfield,california";
	case 662: return "4447161,America/Chicago,starkville,mississippi";
	case 664: return "3578069,America/Barbados,plymouth,montserrat";
	case 667: return "4347820,America/New_York,baltimore,maryland";
	case 669: return "5392171,America/Los_Angeles,san-jose,california";
	case 670: return "7828758,Pacific/Saipan,saipan,northernmarianaislands";
	case 671: return "4043909,Pacific/Saipan,dededo,guam";
	case 672: return "6173331,America/Los_Angeles,vancouver,britishcolumbia";
	case 678: return "4203316,America/New_York,jonesboro,georgia";
	case 679: return "4990510,America/New_York,dearborn,michigan";
	case 680: return "5140405,America/New_York,syracuse,new-york";
	case 681: return "4822764,America/New_York,charleston,west-virginia";
	case 682: return "4691930,America/Chicago,fortworth,texas";
	case 683: return "5009004,America/New_York,saultstemarie,michigan";
	case 684: return "5881576,Pacific/Pago_Pago,pagopago,americansamoa";
	case 689: return "4178573,America/New_York,wintersprings,florida";
	case 701: return "5688025,America/Chicago,bismarck,north-dakota";
	case 702: return "5475433,America/Los_Angeles,lasvegas,nevada";
	case 703: return "4744091,America/New_York,alexandria,virginia";
	case 704: return "4460243,America/New_York,charlotte,northcarolina";
	case 705: return "5009004,America/New_York,saultstemarie,ontario";
	case 706: return "4180531,America/New_York,augusta,georgia";
	case 707: return "5703766,America/Los_Angeles,eureka,california";
	case 708: return "4904381,America/Chicago,oakpark,illinois";
	case 709: return "6354959,America/St_Johns,stjohns,newfoundland";
	case 712: return "4876523,America/Chicago,siouxcity,iowa";
	case 713: return "4699066,America/Chicago,houston,texas";
	case 714: return "5323810,America/Los_Angeles,anaheim,california";
	case 715: return "5268720,America/Chicago,rhinelander,wisconsin";
	case 716: return "5110629,America/New_York,buffalo,new-york";
	case 717: return "5197079,America/New_York,lancaster,pennsylvania";
	case 718: return "5115985,America/New_York,queens,new-york";
	case 719: return "5417598,America/Denver,colorado-springs,colorado";
	case 720: return "5419384,America/Denver,denver,colorado";
	case 721: return "3513392,America/Barbados,philipsburg,sintmaarten";
	case 724: return "5192029,America/New_York,greensburg,pennsylvania";
	case 725: return "5475433,America/Los_Angeles,lasvegas,nevada";
	case 726: return "4726206,America/Chicago,san-antonio,texas";
	case 727: return "4171563,America/New_York,stpetersburg,florida";
	case 731: return "4632595,America/Chicago,jackson,tennessee";
	case 732: return "5101717,America/New_York,newbrunswick,new-jersey";
	case 734: return "4984247,America/New_York,annarbor,michigan";
	case 737: return "4671654,America/Chicago,austin,texas";
	case 740: return "5164466,America/New_York,newark,ohio";
	case 742: return "5969782,America/New_York,hamilton,ontario";
	case 743: return "4469146,America/New_York,greensboro,northcarolina";
	case 747: return "5331835,America/Los_Angeles,burbank,california";
	case 753: return "6094817,America/New_York,ottawa,ontario";
	case 754: return "4155966,America/New_York,fortlauderdale,florida";
	case 757: return "4791259,America/New_York,virginia-beach,virginia";
	case 758: return "3576812,America/Barbados,castries,stlucia";
	case 760: return "5328808,America/Los_Angeles,bishop,california";
	case 762: return "4180531,America/New_York,augusta,georgia";
	case 763: return "5041926,America/Chicago,plymouth,minnesota";
	case 765: return "4922462,America/New_York,lafayette,indiana";
	case 767: return "3575635,America/Barbados,roseau,dominica";
	case 769: return "4431410,America/Chicago,jackson,mississippi";
	case 770: return "4203316,America/New_York,jonesboro,georgia";
	case 772: return "4172372,America/New_York,sebastian,florida";
	case 773: return "4887398,America/Chicago,chicago,illinois";
	case 774: return "4954944,America/New_York,capecod,massachusetts";
	case 775: return "5511077,America/Los_Angeles,reno,nevada";
	case 778: return "6173331,America/Los_Angeles,vancouver,britishcolumbia";
	case 779: return "4907959,America/Chicago,rockford,illinois";
	case 780: return "5946768,America/Denver,edmonton,alberta";
	case 781: return "4943097,America/New_York,marblehead,massachusetts";
	case 782: return "6324729,America/Barbados,halifax,novascotia";
	case 784: return "3577887,America/Barbados,kingstown,stvincentandthegeraldines";
	case 785: return "4280539,America/Chicago,topeka,kansas";
	case 786: return "4164138,America/New_York,miami,florida";
	case 787: return "4568138,America/Barbados,sanjuan,puertorico";
	case 801: return "5780993,America/Denver,saltlakecity,utah";
	case 802: return "5238685,America/New_York,montpelier,vermont";
	case 803: return "4575352,America/New_York,columbia,southcarolina";
	case 804: return "4781708,America/New_York,richmond,virginia";
	case 805: return "5405878,America/Los_Angeles,ventura,california";
	case 806: return "5525577,America/Chicago,lubbock,texas";
	case 807: return "6166142,America/New_York,thunderbay,ontario";
	case 808: return "5856195,Pacific/Honolulu,honolulu,hawaii";
	case 809: return "3508796,America/Barbados,santadomingo,dominican";
	case 810: return "5006233,America/New_York,porthuron,michigan";
	case 812: return "4254679,America/New_York,bloomington,indiana";
	case 813: return "4174757,America/New_York,tampa,florida";
	case 814: return "5188843,America/New_York,erie,pennsylvania";
	case 815: return "4907959,America/Chicago,rockford,illinois";
	case 816: return "4393217,America/Chicago,kansascity,missouri";
	case 817: return "4691930,America/Chicago,fortworth,texas";
	case 818: return "5331835,America/Los_Angeles,burbank,california";
	case 819: return "6146143,America/New_York,sherbrooke,quebec";
	case 820: return "5405878,America/Los_Angeles,ventura,california";
	case 825: return "5946768,America/Denver,edmonton,alberta";
	case 828: return "4453066,America/New_York,asheville,northcarolina";
	case 829: return "3508796,America/Barbados,santadomingo,dominican";
	case 830: return "5520076,America/Chicago,delrio,texas";
	case 831: return "5393052,America/Los_Angeles,santacruz,california";
	case 832: return "4699066,America/Chicago,houston,texas";
	case 838: return "5106834,America/New_York,albany,new-york";
	case 839: return "4575352,America/New_York,columbia,southcarolina";
	case 843: return "4574324,America/New_York,charleston,southcarolina";
	case 845: return "5132143,America/New_York,poughkeepsie,new-york";
	case 847: return "4890864,America/Chicago,elgin,illinois";
	case 848: return "5101717,America/New_York,newbrunswick,new-jersey";
	case 849: return "3508796,America/Barbados,santadomingo,dominican";
	case 850: return "4174715,America/New_York,tallahassee,florida";
	case 854: return "4574324,America/New_York,charleston,southcarolina";
	case 856: return "4501018,America/New_York,camden,new-jersey";
	case 857: return "4930956,America/New_York,boston,massachusetts";
	case 858: return "5391811,America/Los_Angeles,san-diego,california";
	case 859: return "4297983,America/New_York,lexington,kentucky";
	case 860: return "4835797,America/New_York,hartford,connecticut";
	case 862: return "5101798,America/New_York,newark,new-jersey";
	case 863: return "4204775,America/New_York,lakeland,florida";
	case 864: return "4580543,America/New_York,greenville,southcarolina";
	case 865: return "4634946,America/New_York,knoxville,tennessee";
	case 867: return "6180550,America/Los_Angeles,whitehorse,yukon";
	case 868: return "7521937,America/Barbados,chaguanas,trinidad";
	case 869: return "3575551,America/Barbados,basseterre,stkitts";
	case 870: return "4116834,America/Chicago,jonesboro,arkansas";
	case 872: return "4887398,America/Chicago,chicago,illinois";
	case 873: return "6146143,America/New_York,sherbrooke,quebec";
	case 876: return "3489854,America/New_York,kingston,jamaica";
	case 878: return "5192029,America/New_York,greensburg,pennsylvania";
	case 879: return "6354959,America/St_Johns,stjohns,newfoundland";
	case 901: return "4641239,America/Chicago,memphis,tennessee";
	case 902: return "6324729,America/Barbados,halifax,novascotia";
	case 903: return "4738214,America/Chicago,tyler,texas";
	case 904: return "4160021,America/New_York,jacksonville,florida";
	case 905: return "5969782,America/New_York,hamilton,ontario";
	case 906: return "5000947,America/New_York,marquette,michigan";
	case 907: return "5879400,America/Anchorage,anchorage,alaska";
	case 908: return "5097598,America/New_York,elizabeth,new-jersey";
	case 909: return "5349755,America/Los_Angeles,fontana,california";
	case 910: return "4466033,America/New_York,fayetteville,northcarolina";
	case 912: return "4221552,America/New_York,savannah,georgia";
	case 913: return "4273837,America/Chicago,kansascity,kansas";
	case 914: return "5145215,America/New_York,yonkers,new-york";
	case 915: return "5520993,America/Denver,elpaso,texas";
	case 916: return "5389489,America/Los_Angeles,sacramento,california";
	case 917: return "5110253,America/New_York,bronx,new-york";
	case 918: return "4553433,America/Chicago,tulsa,oklahoma";
	case 919: return "4487042,America/New_York,raleigh,northcarolina";
	case 920: return "5254962,America/Chicago,greenbay,wisconsin";
	case 925: return "5367440,America/Los_Angeles,livermore,california";
	case 927: return "4167147,America/New_York,orlando,florida";
	case 928: return "5294810,America/Phoenix,flagstaff,arizona";
	case 929: return "5115985,America/New_York,queens,new-york";
	case 930: return "4254679,America/New_York,bloomington,indiana";
	case 931: return "4613868,America/Chicago,clarksville,tennessee";
	case 934: return "5127561,America/New_York,morrisville,new-york";
	case 936: return "4699540,America/Chicago,huntsville,texas";
	case 937: return "4289629,America/New_York,dayton,ohio";
	case 938: return "4068590,America/Chicago,huntsville,alabama";
	case 939: return "4568138,America/Barbados,sanjuan,puertorico";
	case 940: return "4685907,America/Chicago,denton,texas";
	case 941: return "4172131,America/New_York,sarasota,florida";
	case 942: return "6167863,America/New_York,toronto,ontario";
	case 947: return "5006166,America/New_York,pontiac,michigan";
	case 949: return "5359777,America/Los_Angeles,irvine,california";
	case 951: return "5387877,America/Los_Angeles,riverside,california";
	case 952: return "7506758,America/Chicago,bloomington,minnesota";
	case 954: return "4155966,America/New_York,fortlauderdale,florida";
	case 956: return "4705349,America/Chicago,laredo,texas";
	case 959: return "4835797,America/New_York,hartford,connecticut";
	case 970: return "5423573,America/Denver,grandjunction,colorado";
	case 971: return "5746545,America/Los_Angeles,portland,oregon";
	case 972: return "4190598,America/Chicago,dallas,texas";
	case 973: return "5101798,America/New_York,newark,new-jersey";
	case 978: return "4942618,America/New_York,lowell,massachusetts";
	case 979: return "4741325,America/Chicago,wharton,texas";
	case 980: return "4460243,America/New_York,charlotte,northcarolina";
	case 984: return "4487042,America/New_York,raleigh,northcarolina";
	case 985: return "4328010,America/Chicago,houma,louisiana";
	case 986: return "5586437,America/Denver,boise,idaho";
	case 989: return "5007989,America/New_York,saginaw,michigan";
	default:
		ast_assert_return(NULL, 0);
	}
}

static inline int npa_locale_info(int npa, int index, char *buf, size_t maxlen)
{
	int i;
	char *dup, *tmp;
	const char *info = npa_locale(npa);
	if (!info) {
		return -1;
	}
	dup = ast_strdupa(info);
	for (i = 0; i < index; i++) {
		strsep(&dup, ",");
	}
	tmp = strchr(dup, ',');
	if (tmp) {
		*tmp = '\0';
	}
	ast_copy_string(buf, dup, maxlen);
	return 0;
}

static int func_nanpa_read(struct ast_channel *chan, const char *function, char *data, char *buf, size_t maxlen)
{
	char *argcopy = NULL;
	int npa, base;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(npa);
		AST_APP_ARG(property);
	);

	*buf = '\0';

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "%s: NPA required\n", function);
		return -1;
	}

	argcopy = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, argcopy);

	if (ast_strlen_zero(args.npa)) {
		ast_log(LOG_WARNING, "%s: NPA required\n", function);
		return -1;
	} else if (ast_strlen_zero(args.property)) {
		ast_log(LOG_WARNING, "%s: Property required\n", function);
		return -1;
	}

	npa = atoi(args.npa);
	base = npa_base(npa);

	/* Screen out invalid NPAs first. */
	if (base < 0) {
		ast_log(LOG_WARNING, "Invalid/unknown NPA: %d\n", npa);
		return -1;
	}

	if (!strcasecmp(args.property, "base")) {
		snprintf(buf, maxlen, "%d", base);
	} else if (!strcasecmp(args.property, "overlaid")) {
		snprintf(buf, maxlen, "%d", npa_overlaid(npa));
	} else if (!strcasecmp(args.property, "zip")) {
		/* Use the base area code for the lookup. */
		snprintf(buf, maxlen, "%s", npa_zip(base));
	} else if (!strcasecmp(args.property, "cityid")) {
		return npa_locale_info(base, 0, buf, maxlen);
	} else if (!strcasecmp(args.property, "tz")) {
		return npa_locale_info(base, 1, buf, maxlen);
	} else if (!strcasecmp(args.property, "city")) {
		return npa_locale_info(base, 2, buf, maxlen);
	} else if (!strcasecmp(args.property, "state")) {
		return npa_locale_info(base, 3, buf, maxlen);
	} else {
		ast_log(LOG_WARNING, "Invalid property: %s\n", args.property);
		return -1;
	}

	return 0;
}

static struct ast_custom_function nanpa_function = {
	.name = "NANPA",
	.read = func_nanpa_read,
};

static int unload_module(void)
{
	return ast_custom_function_unregister(&nanpa_function);
}

static int load_module(void)
{
	return ast_custom_function_register(&nanpa_function);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "NANPA Number Information");
