/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "../testeventloop.h"
#include <apl/utils/url.h>

using namespace apl;

class UrlTest : public MemoryWrapper {};

static const char* DECODED_URL = "https://tools.ietf.org/html/rfc3986#section-2.4";
static const char* ENCODED_URL = "https%3A%2F%2Ftools.ietf.org%2Fhtml%2Frfc3986%23section-2.4";
static const char* UNICODE_URL = "http://müsic.example/motörhead";
static const char* ENCODED_UNICODE_URL = "http%3A%2F%2Fm%C3%BCsic.example%2Fmot%C3%B6rhead";
// about 1700 glyphs
static const char* LONG_UNICODE_STRING = "援マ提欲フヨカ対円はわらを益62始54布ルモロ躍度に保共ぶぐ挙年たゃがを子団ッ刊需ぱれざめ総道組作久払ひどみへ。雑モチ毒歌7浜どせきの演欧ロエ飲天ムミツハ毎破ニトノ催種ヘオロス列卒ひな年崎ひの的区フ文若イリめに念団ヱムホ能由当るスしッ詳回うにぼ被41時おょ治国キ入英距えくけ。厚合ル芸介ツロカ作6文贅クフ使長ぜゃぽン田空ヌキソ女種イ足負ばっしち俊資はみくと之省め助協審営持のおイだ。梨せほリド芸革町ミカシト応処ナレツロ同井ルえき息際負レナヱ大社45求ラリス康51望ルケクセ散式ヌクマ傍者夜た名社ヘオス送想ソハモ事景染お。禁ヨヲチツ準省北ねょリが度九東そ歩質ゆどすぎ紙訪イテサ学多ラッで望害ドそ陸大めら進小ちど者験レ日再チネノ投攻かつ率追しく可5定がぶざず展応クネウコ必級支ぽじ。年レ図団けす繰3要ぱぼっる装姿らひぞ字国団ヌミヤ目社頭慮がつ陸性れ南7後べ年焦茂勤複ぴもがせ。件が球転シロ約体おね台陣ケホ宇4聞ルケ特羅むぶひ界火ケヌ察東けずゃ推流シテエ月更ヘフ要努よクをほ打暮ネソ知土ヤメ光8遣便シヒ界風む持死央紹舞ゅ。意メマ判化部統ナ意誘じかひ明違わフん能男っこ諸相だみ特年フユヘ十長食は国速ウア機建ンやほラ支月校掃邸ルみざ。差一シレヱヌ文化ろて止多ゃむ送酒館がちをぜ一院かおい結4町ラ時混メソアサ志投ケワカ手6人う護整そ票鹿とか写考升だッぴ。32治ぐうめ阜故セ紙室づ期属ぴ聞揚レセ調谷あひも純携チロマ観戦憲リ告公フカロ事質ユソタ警真筋神ぞなス確1更リ計券ヤカ辞噴れみ。情ヤツ文細催イヒ際年ラモ月速ヲサリフ旅統紙47医ざ経乗ふ恐務ヤニフ級南ほべずお覧朝数卓ぞレ。委護が本事イ見意ーしぜほ禁康リ浮募ムホコハ逮記ド先健ヨ加2今コイケ更情だ択4自ネホカ真展探軽参レらも。務ドびも実球京問サヘチ軟92望写面ハト繕亡ム川低著ケメラユ映身アウサヨ北逃ウ勝松くぎ表午微酸五断ろ。愛ずのめ毎際社てりへ応目しょゅ雪位ワミイメ送見ょじフ様朝へつむぞ州政れレけ思可ミウイヱ並40文エ加3変フ細入うむ葉松さ。岩ぶ銀和ツ需能ぼたッレ供健塚スほぼゆ持換ラ置宏ミヲモネ響画チリ紙暮ユハキ作要フ科73唯ウスツネ写昇べ。年へて停子レミヌナ葉出4互ヨニモ赤村ラこらり笑劇トコマ殺県さたしあ二果ほべ死禁サエ簡際ツ夜30引くつぜ歳90侃凸ッリたつ。松渡撃速移いわあ予投だとやぽ優2感ロモマチ点抄ソル道作止学フストナ見虫ゆねぶ刀表っ越権集え転標つね揚新び英必ロフト向侍びへー。主ウヒコ企含園ルードせ氷歓え後写そ際受もまく審度す米記ヱトセス写伝ドね老作ヘ必96視モテ第入ラタスコ約氷ッこ減為ヲウ報年ゃ元益へ応株玉治供ぱ。類見レムウソ課要リ理題ぱるトぜ結使でぶ業1覧4局告ネアモ漢5悩しいて急敢米れ。念ヘフ秘新じ始真キ席厚ヤ見聴はっリ節小んりそ連著ぴぼ与線ツロ康任タテオノ強販学テ吉無中こ禁備戦びくさぽ在募73協基ざれ。際シリコミ本稼ヲナリ言見にたリ服4新あ州速雪ひまめ都発多ラツソ触童よげスら掲院おんひは彩面るフリた進目亭匠な。歩こ売8画フチ合信ネアノ必強ミ社伝ケ肥歩ほだえイ沖信クムメ第問クて年象覧行8別ふにづ島合ツフ掲増ら借指ハ切定しつ前場ーちもめ。笹へきぎれ佐詳づさぶ岡著ヱマタヘ企旅容ネアムホ小選拠がらドか節東くち構一フソスワ碁入タムニミ事権ケチワ秘血形童めドり和定えうにい球知囲わこじ。道をほ泳借ひん稿藪だ識座ナカシ同待無ぎ暖21利ひぽ理元でうち車食ミナレユ悩身訪テキソ気在ユ位4予テシウヒ芸41戸あどばろ支2残ろ年知リナ永歯没牧せめしろ。観ネホラ子発ヌ実銀フもさり炭議信ミラレ安委フ朝48事いん産引ワ息上ろもすを囲犯ド国藤オホ米町機セヨ社退らちぼ録調題ら。演任ねスび供未ゆねえラ検遺カセ像援寿消継ンりは東要トど基静モナ感国ロラフワ当聞変昇むそひぴ。同ゃーのに島北ム読携ヲミ東文族ロ師16入うぶ選空飾たゃ毎24優職シ直込げゅ魚感計81機ア由必囲ムサチ出2職群てたとち。養セコ割場たよる込方ソ必午社えれゆ稿禁をるゅ完禁ドろやつ有応ヱフ細刀ぶふぎ浪並あれが多治ツキマカ手委を巣終ぞ置3選ノア由宅そてつま室王クとべく議報とークが会煙畿るス。";
// explode to about 15k encoded characters
static const char* ENCODED_LONG_UNICODE_STRING = "%E6%8F%B4%E3%83%9E%E6%8F%90%E6%AC%B2%E3%83%95%E3%83%A8%E3%82%AB%E5%AF%BE%E5%86%86%E3%81%AF%E3%82%8F%E3%82%89%E3%82%92%E7%9B%8A62%E5%A7%8B54%E5%B8%83%E3%83%AB%E3%83%A2%E3%83%AD%E8%BA%8D%E5%BA%A6%E3%81%AB%E4%BF%9D%E5%85%B1%E3%81%B6%E3%81%90%E6%8C%99%E5%B9%B4%E3%81%9F%E3%82%83%E3%81%8C%E3%82%92%E5%AD%90%E5%9B%A3%E3%83%83%E5%88%8A%E9%9C%80%E3%81%B1%E3%82%8C%E3%81%96%E3%82%81%E7%B7%8F%E9%81%93%E7%B5%84%E4%BD%9C%E4%B9%85%E6%89%95%E3%81%B2%E3%81%A9%E3%81%BF%E3%81%B8%E3%80%82%E9%9B%91%E3%83%A2%E3%83%81%E6%AF%92%E6%AD%8C7%E6%B5%9C%E3%81%A9%E3%81%9B%E3%81%8D%E3%81%AE%E6%BC%94%E6%AC%A7%E3%83%AD%E3%82%A8%E9%A3%B2%E5%A4%A9%E3%83%A0%E3%83%9F%E3%83%84%E3%83%8F%E6%AF%8E%E7%A0%B4%E3%83%8B%E3%83%88%E3%83%8E%E5%82%AC%E7%A8%AE%E3%83%98%E3%82%AA%E3%83%AD%E3%82%B9%E5%88%97%E5%8D%92%E3%81%B2%E3%81%AA%E5%B9%B4%E5%B4%8E%E3%81%B2%E3%81%AE%E7%9A%84%E5%8C%BA%E3%83%95%E6%96%87%E8%8B%A5%E3%82%A4%E3%83%AA%E3%82%81%E3%81%AB%E5%BF%B5%E5%9B%A3%E3%83%B1%E3%83%A0%E3%83%9B%E8%83%BD%E7%94%B1%E5%BD%93%E3%82%8B%E3%82%B9%E3%81%97%E3%83%83%E8%A9%B3%E5%9B%9E%E3%81%86%E3%81%AB%E3%81%BC%E8%A2%AB41%E6%99%82%E3%81%8A%E3%82%87%E6%B2%BB%E5%9B%BD%E3%82%AD%E5%85%A5%E8%8B%B1%E8%B7%9D%E3%81%88%E3%81%8F%E3%81%91%E3%80%82%E5%8E%9A%E5%90%88%E3%83%AB%E8%8A%B8%E4%BB%8B%E3%83%84%E3%83%AD%E3%82%AB%E4%BD%9C6%E6%96%87%E8%B4%85%E3%82%AF%E3%83%95%E4%BD%BF%E9%95%B7%E3%81%9C%E3%82%83%E3%81%BD%E3%83%B3%E7%94%B0%E7%A9%BA%E3%83%8C%E3%82%AD%E3%82%BD%E5%A5%B3%E7%A8%AE%E3%82%A4%E8%B6%B3%E8%B2%A0%E3%81%B0%E3%81%A3%E3%81%97%E3%81%A1%E4%BF%8A%E8%B3%87%E3%81%AF%E3%81%BF%E3%81%8F%E3%81%A8%E4%B9%8B%E7%9C%81%E3%82%81%E5%8A%A9%E5%8D%94%E5%AF%A9%E5%96%B6%E6%8C%81%E3%81%AE%E3%81%8A%E3%82%A4%E3%81%A0%E3%80%82%E6%A2%A8%E3%81%9B%E3%81%BB%E3%83%AA%E3%83%89%E8%8A%B8%E9%9D%A9%E7%94%BA%E3%83%9F%E3%82%AB%E3%82%B7%E3%83%88%E5%BF%9C%E5%87%A6%E3%83%8A%E3%83%AC%E3%83%84%E3%83%AD%E5%90%8C%E4%BA%95%E3%83%AB%E3%81%88%E3%81%8D%E6%81%AF%E9%9A%9B%E8%B2%A0%E3%83%AC%E3%83%8A%E3%83%B1%E5%A4%A7%E7%A4%BE45%E6%B1%82%E3%83%A9%E3%83%AA%E3%82%B9%E5%BA%B751%E6%9C%9B%E3%83%AB%E3%82%B1%E3%82%AF%E3%82%BB%E6%95%A3%E5%BC%8F%E3%83%8C%E3%82%AF%E3%83%9E%E5%82%8D%E8%80%85%E5%A4%9C%E3%81%9F%E5%90%8D%E7%A4%BE%E3%83%98%E3%82%AA%E3%82%B9%E9%80%81%E6%83%B3%E3%82%BD%E3%83%8F%E3%83%A2%E4%BA%8B%E6%99%AF%E6%9F%93%E3%81%8A%E3%80%82%E7%A6%81%E3%83%A8%E3%83%B2%E3%83%81%E3%83%84%E6%BA%96%E7%9C%81%E5%8C%97%E3%81%AD%E3%82%87%E3%83%AA%E3%81%8C%E5%BA%A6%E4%B9%9D%E6%9D%B1%E3%81%9D%E6%AD%A9%E8%B3%AA%E3%82%86%E3%81%A9%E3%81%99%E3%81%8E%E7%B4%99%E8%A8%AA%E3%82%A4%E3%83%86%E3%82%B5%E5%AD%A6%E5%A4%9A%E3%83%A9%E3%83%83%E3%81%A7%E6%9C%9B%E5%AE%B3%E3%83%89%E3%81%9D%E9%99%B8%E5%A4%A7%E3%82%81%E3%82%89%E9%80%B2%E5%B0%8F%E3%81%A1%E3%81%A9%E8%80%85%E9%A8%93%E3%83%AC%E6%97%A5%E5%86%8D%E3%83%81%E3%83%8D%E3%83%8E%E6%8A%95%E6%94%BB%E3%81%8B%E3%81%A4%E7%8E%87%E8%BF%BD%E3%81%97%E3%81%8F%E5%8F%AF5%E5%AE%9A%E3%81%8C%E3%81%B6%E3%81%96%E3%81%9A%E5%B1%95%E5%BF%9C%E3%82%AF%E3%83%8D%E3%82%A6%E3%82%B3%E5%BF%85%E7%B4%9A%E6%94%AF%E3%81%BD%E3%81%98%E3%80%82%E5%B9%B4%E3%83%AC%E5%9B%B3%E5%9B%A3%E3%81%91%E3%81%99%E7%B9%B03%E8%A6%81%E3%81%B1%E3%81%BC%E3%81%A3%E3%82%8B%E8%A3%85%E5%A7%BF%E3%82%89%E3%81%B2%E3%81%9E%E5%AD%97%E5%9B%BD%E5%9B%A3%E3%83%8C%E3%83%9F%E3%83%A4%E7%9B%AE%E7%A4%BE%E9%A0%AD%E6%85%AE%E3%81%8C%E3%81%A4%E9%99%B8%E6%80%A7%E3%82%8C%E5%8D%977%E5%BE%8C%E3%81%B9%E5%B9%B4%E7%84%A6%E8%8C%82%E5%8B%A4%E8%A4%87%E3%81%B4%E3%82%82%E3%81%8C%E3%81%9B%E3%80%82%E4%BB%B6%E3%81%8C%E7%90%83%E8%BB%A2%E3%82%B7%E3%83%AD%E7%B4%84%E4%BD%93%E3%81%8A%E3%81%AD%E5%8F%B0%E9%99%A3%E3%82%B1%E3%83%9B%E5%AE%874%E8%81%9E%E3%83%AB%E3%82%B1%E7%89%B9%E7%BE%85%E3%82%80%E3%81%B6%E3%81%B2%E7%95%8C%E7%81%AB%E3%82%B1%E3%83%8C%E5%AF%9F%E6%9D%B1%E3%81%91%E3%81%9A%E3%82%83%E6%8E%A8%E6%B5%81%E3%82%B7%E3%83%86%E3%82%A8%E6%9C%88%E6%9B%B4%E3%83%98%E3%83%95%E8%A6%81%E5%8A%AA%E3%82%88%E3%82%AF%E3%82%92%E3%81%BB%E6%89%93%E6%9A%AE%E3%83%8D%E3%82%BD%E7%9F%A5%E5%9C%9F%E3%83%A4%E3%83%A1%E5%85%898%E9%81%A3%E4%BE%BF%E3%82%B7%E3%83%92%E7%95%8C%E9%A2%A8%E3%82%80%E6%8C%81%E6%AD%BB%E5%A4%AE%E7%B4%B9%E8%88%9E%E3%82%85%E3%80%82%E6%84%8F%E3%83%A1%E3%83%9E%E5%88%A4%E5%8C%96%E9%83%A8%E7%B5%B1%E3%83%8A%E6%84%8F%E8%AA%98%E3%81%98%E3%81%8B%E3%81%B2%E6%98%8E%E9%81%95%E3%82%8F%E3%83%95%E3%82%93%E8%83%BD%E7%94%B7%E3%81%A3%E3%81%93%E8%AB%B8%E7%9B%B8%E3%81%A0%E3%81%BF%E7%89%B9%E5%B9%B4%E3%83%95%E3%83%A6%E3%83%98%E5%8D%81%E9%95%B7%E9%A3%9F%E3%81%AF%E5%9B%BD%E9%80%9F%E3%82%A6%E3%82%A2%E6%A9%9F%E5%BB%BA%E3%83%B3%E3%82%84%E3%81%BB%E3%83%A9%E6%94%AF%E6%9C%88%E6%A0%A1%E6%8E%83%E9%82%B8%E3%83%AB%E3%81%BF%E3%81%96%E3%80%82%E5%B7%AE%E4%B8%80%E3%82%B7%E3%83%AC%E3%83%B1%E3%83%8C%E6%96%87%E5%8C%96%E3%82%8D%E3%81%A6%E6%AD%A2%E5%A4%9A%E3%82%83%E3%82%80%E9%80%81%E9%85%92%E9%A4%A8%E3%81%8C%E3%81%A1%E3%82%92%E3%81%9C%E4%B8%80%E9%99%A2%E3%81%8B%E3%81%8A%E3%81%84%E7%B5%904%E7%94%BA%E3%83%A9%E6%99%82%E6%B7%B7%E3%83%A1%E3%82%BD%E3%82%A2%E3%82%B5%E5%BF%97%E6%8A%95%E3%82%B1%E3%83%AF%E3%82%AB%E6%89%8B6%E4%BA%BA%E3%81%86%E8%AD%B7%E6%95%B4%E3%81%9D%E7%A5%A8%E9%B9%BF%E3%81%A8%E3%81%8B%E5%86%99%E8%80%83%E5%8D%87%E3%81%A0%E3%83%83%E3%81%B4%E3%80%8232%E6%B2%BB%E3%81%90%E3%81%86%E3%82%81%E9%98%9C%E6%95%85%E3%82%BB%E7%B4%99%E5%AE%A4%E3%81%A5%E6%9C%9F%E5%B1%9E%E3%81%B4%E8%81%9E%E6%8F%9A%E3%83%AC%E3%82%BB%E8%AA%BF%E8%B0%B7%E3%81%82%E3%81%B2%E3%82%82%E7%B4%94%E6%90%BA%E3%83%81%E3%83%AD%E3%83%9E%E8%A6%B3%E6%88%A6%E6%86%B2%E3%83%AA%E5%91%8A%E5%85%AC%E3%83%95%E3%82%AB%E3%83%AD%E4%BA%8B%E8%B3%AA%E3%83%A6%E3%82%BD%E3%82%BF%E8%AD%A6%E7%9C%9F%E7%AD%8B%E7%A5%9E%E3%81%9E%E3%81%AA%E3%82%B9%E7%A2%BA1%E6%9B%B4%E3%83%AA%E8%A8%88%E5%88%B8%E3%83%A4%E3%82%AB%E8%BE%9E%E5%99%B4%E3%82%8C%E3%81%BF%E3%80%82%E6%83%85%E3%83%A4%E3%83%84%E6%96%87%E7%B4%B0%E5%82%AC%E3%82%A4%E3%83%92%E9%9A%9B%E5%B9%B4%E3%83%A9%E3%83%A2%E6%9C%88%E9%80%9F%E3%83%B2%E3%82%B5%E3%83%AA%E3%83%95%E6%97%85%E7%B5%B1%E7%B4%9947%E5%8C%BB%E3%81%96%E7%B5%8C%E4%B9%97%E3%81%B5%E6%81%90%E5%8B%99%E3%83%A4%E3%83%8B%E3%83%95%E7%B4%9A%E5%8D%97%E3%81%BB%E3%81%B9%E3%81%9A%E3%81%8A%E8%A6%A7%E6%9C%9D%E6%95%B0%E5%8D%93%E3%81%9E%E3%83%AC%E3%80%82%E5%A7%94%E8%AD%B7%E3%81%8C%E6%9C%AC%E4%BA%8B%E3%82%A4%E8%A6%8B%E6%84%8F%E3%83%BC%E3%81%97%E3%81%9C%E3%81%BB%E7%A6%81%E5%BA%B7%E3%83%AA%E6%B5%AE%E5%8B%9F%E3%83%A0%E3%83%9B%E3%82%B3%E3%83%8F%E9%80%AE%E8%A8%98%E3%83%89%E5%85%88%E5%81%A5%E3%83%A8%E5%8A%A02%E4%BB%8A%E3%82%B3%E3%82%A4%E3%82%B1%E6%9B%B4%E6%83%85%E3%81%A0%E6%8A%9E4%E8%87%AA%E3%83%8D%E3%83%9B%E3%82%AB%E7%9C%9F%E5%B1%95%E6%8E%A2%E8%BB%BD%E5%8F%82%E3%83%AC%E3%82%89%E3%82%82%E3%80%82%E5%8B%99%E3%83%89%E3%81%B3%E3%82%82%E5%AE%9F%E7%90%83%E4%BA%AC%E5%95%8F%E3%82%B5%E3%83%98%E3%83%81%E8%BB%9F92%E6%9C%9B%E5%86%99%E9%9D%A2%E3%83%8F%E3%83%88%E7%B9%95%E4%BA%A1%E3%83%A0%E5%B7%9D%E4%BD%8E%E8%91%97%E3%82%B1%E3%83%A1%E3%83%A9%E3%83%A6%E6%98%A0%E8%BA%AB%E3%82%A2%E3%82%A6%E3%82%B5%E3%83%A8%E5%8C%97%E9%80%83%E3%82%A6%E5%8B%9D%E6%9D%BE%E3%81%8F%E3%81%8E%E8%A1%A8%E5%8D%88%E5%BE%AE%E9%85%B8%E4%BA%94%E6%96%AD%E3%82%8D%E3%80%82%E6%84%9B%E3%81%9A%E3%81%AE%E3%82%81%E6%AF%8E%E9%9A%9B%E7%A4%BE%E3%81%A6%E3%82%8A%E3%81%B8%E5%BF%9C%E7%9B%AE%E3%81%97%E3%82%87%E3%82%85%E9%9B%AA%E4%BD%8D%E3%83%AF%E3%83%9F%E3%82%A4%E3%83%A1%E9%80%81%E8%A6%8B%E3%82%87%E3%81%98%E3%83%95%E6%A7%98%E6%9C%9D%E3%81%B8%E3%81%A4%E3%82%80%E3%81%9E%E5%B7%9E%E6%94%BF%E3%82%8C%E3%83%AC%E3%81%91%E6%80%9D%E5%8F%AF%E3%83%9F%E3%82%A6%E3%82%A4%E3%83%B1%E4%B8%A640%E6%96%87%E3%82%A8%E5%8A%A03%E5%A4%89%E3%83%95%E7%B4%B0%E5%85%A5%E3%81%86%E3%82%80%E8%91%89%E6%9D%BE%E3%81%95%E3%80%82%E5%B2%A9%E3%81%B6%E9%8A%80%E5%92%8C%E3%83%84%E9%9C%80%E8%83%BD%E3%81%BC%E3%81%9F%E3%83%83%E3%83%AC%E4%BE%9B%E5%81%A5%E5%A1%9A%E3%82%B9%E3%81%BB%E3%81%BC%E3%82%86%E6%8C%81%E6%8F%9B%E3%83%A9%E7%BD%AE%E5%AE%8F%E3%83%9F%E3%83%B2%E3%83%A2%E3%83%8D%E9%9F%BF%E7%94%BB%E3%83%81%E3%83%AA%E7%B4%99%E6%9A%AE%E3%83%A6%E3%83%8F%E3%82%AD%E4%BD%9C%E8%A6%81%E3%83%95%E7%A7%9173%E5%94%AF%E3%82%A6%E3%82%B9%E3%83%84%E3%83%8D%E5%86%99%E6%98%87%E3%81%B9%E3%80%82%E5%B9%B4%E3%81%B8%E3%81%A6%E5%81%9C%E5%AD%90%E3%83%AC%E3%83%9F%E3%83%8C%E3%83%8A%E8%91%89%E5%87%BA4%E4%BA%92%E3%83%A8%E3%83%8B%E3%83%A2%E8%B5%A4%E6%9D%91%E3%83%A9%E3%81%93%E3%82%89%E3%82%8A%E7%AC%91%E5%8A%87%E3%83%88%E3%82%B3%E3%83%9E%E6%AE%BA%E7%9C%8C%E3%81%95%E3%81%9F%E3%81%97%E3%81%82%E4%BA%8C%E6%9E%9C%E3%81%BB%E3%81%B9%E6%AD%BB%E7%A6%81%E3%82%B5%E3%82%A8%E7%B0%A1%E9%9A%9B%E3%83%84%E5%A4%9C30%E5%BC%95%E3%81%8F%E3%81%A4%E3%81%9C%E6%AD%B390%E4%BE%83%E5%87%B8%E3%83%83%E3%83%AA%E3%81%9F%E3%81%A4%E3%80%82%E6%9D%BE%E6%B8%A1%E6%92%83%E9%80%9F%E7%A7%BB%E3%81%84%E3%82%8F%E3%81%82%E4%BA%88%E6%8A%95%E3%81%A0%E3%81%A8%E3%82%84%E3%81%BD%E5%84%AA2%E6%84%9F%E3%83%AD%E3%83%A2%E3%83%9E%E3%83%81%E7%82%B9%E6%8A%84%E3%82%BD%E3%83%AB%E9%81%93%E4%BD%9C%E6%AD%A2%E5%AD%A6%E3%83%95%E3%82%B9%E3%83%88%E3%83%8A%E8%A6%8B%E8%99%AB%E3%82%86%E3%81%AD%E3%81%B6%E5%88%80%E8%A1%A8%E3%81%A3%E8%B6%8A%E6%A8%A9%E9%9B%86%E3%81%88%E8%BB%A2%E6%A8%99%E3%81%A4%E3%81%AD%E6%8F%9A%E6%96%B0%E3%81%B3%E8%8B%B1%E5%BF%85%E3%83%AD%E3%83%95%E3%83%88%E5%90%91%E4%BE%8D%E3%81%B3%E3%81%B8%E3%83%BC%E3%80%82%E4%B8%BB%E3%82%A6%E3%83%92%E3%82%B3%E4%BC%81%E5%90%AB%E5%9C%92%E3%83%AB%E3%83%BC%E3%83%89%E3%81%9B%E6%B0%B7%E6%AD%93%E3%81%88%E5%BE%8C%E5%86%99%E3%81%9D%E9%9A%9B%E5%8F%97%E3%82%82%E3%81%BE%E3%81%8F%E5%AF%A9%E5%BA%A6%E3%81%99%E7%B1%B3%E8%A8%98%E3%83%B1%E3%83%88%E3%82%BB%E3%82%B9%E5%86%99%E4%BC%9D%E3%83%89%E3%81%AD%E8%80%81%E4%BD%9C%E3%83%98%E5%BF%8596%E8%A6%96%E3%83%A2%E3%83%86%E7%AC%AC%E5%85%A5%E3%83%A9%E3%82%BF%E3%82%B9%E3%82%B3%E7%B4%84%E6%B0%B7%E3%83%83%E3%81%93%E6%B8%9B%E7%82%BA%E3%83%B2%E3%82%A6%E5%A0%B1%E5%B9%B4%E3%82%83%E5%85%83%E7%9B%8A%E3%81%B8%E5%BF%9C%E6%A0%AA%E7%8E%89%E6%B2%BB%E4%BE%9B%E3%81%B1%E3%80%82%E9%A1%9E%E8%A6%8B%E3%83%AC%E3%83%A0%E3%82%A6%E3%82%BD%E8%AA%B2%E8%A6%81%E3%83%AA%E7%90%86%E9%A1%8C%E3%81%B1%E3%82%8B%E3%83%88%E3%81%9C%E7%B5%90%E4%BD%BF%E3%81%A7%E3%81%B6%E6%A5%AD1%E8%A6%A74%E5%B1%80%E5%91%8A%E3%83%8D%E3%82%A2%E3%83%A2%E6%BC%A25%E6%82%A9%E3%81%97%E3%81%84%E3%81%A6%E6%80%A5%E6%95%A2%E7%B1%B3%E3%82%8C%E3%80%82%E5%BF%B5%E3%83%98%E3%83%95%E7%A7%98%E6%96%B0%E3%81%98%E5%A7%8B%E7%9C%9F%E3%82%AD%E5%B8%AD%E5%8E%9A%E3%83%A4%E8%A6%8B%E8%81%B4%E3%81%AF%E3%81%A3%E3%83%AA%E7%AF%80%E5%B0%8F%E3%82%93%E3%82%8A%E3%81%9D%E9%80%A3%E8%91%97%E3%81%B4%E3%81%BC%E4%B8%8E%E7%B7%9A%E3%83%84%E3%83%AD%E5%BA%B7%E4%BB%BB%E3%82%BF%E3%83%86%E3%82%AA%E3%83%8E%E5%BC%B7%E8%B2%A9%E5%AD%A6%E3%83%86%E5%90%89%E7%84%A1%E4%B8%AD%E3%81%93%E7%A6%81%E5%82%99%E6%88%A6%E3%81%B3%E3%81%8F%E3%81%95%E3%81%BD%E5%9C%A8%E5%8B%9F73%E5%8D%94%E5%9F%BA%E3%81%96%E3%82%8C%E3%80%82%E9%9A%9B%E3%82%B7%E3%83%AA%E3%82%B3%E3%83%9F%E6%9C%AC%E7%A8%BC%E3%83%B2%E3%83%8A%E3%83%AA%E8%A8%80%E8%A6%8B%E3%81%AB%E3%81%9F%E3%83%AA%E6%9C%8D4%E6%96%B0%E3%81%82%E5%B7%9E%E9%80%9F%E9%9B%AA%E3%81%B2%E3%81%BE%E3%82%81%E9%83%BD%E7%99%BA%E5%A4%9A%E3%83%A9%E3%83%84%E3%82%BD%E8%A7%A6%E7%AB%A5%E3%82%88%E3%81%92%E3%82%B9%E3%82%89%E6%8E%B2%E9%99%A2%E3%81%8A%E3%82%93%E3%81%B2%E3%81%AF%E5%BD%A9%E9%9D%A2%E3%82%8B%E3%83%95%E3%83%AA%E3%81%9F%E9%80%B2%E7%9B%AE%E4%BA%AD%E5%8C%A0%E3%81%AA%E3%80%82%E6%AD%A9%E3%81%93%E5%A3%B28%E7%94%BB%E3%83%95%E3%83%81%E5%90%88%E4%BF%A1%E3%83%8D%E3%82%A2%E3%83%8E%E5%BF%85%E5%BC%B7%E3%83%9F%E7%A4%BE%E4%BC%9D%E3%82%B1%E8%82%A5%E6%AD%A9%E3%81%BB%E3%81%A0%E3%81%88%E3%82%A4%E6%B2%96%E4%BF%A1%E3%82%AF%E3%83%A0%E3%83%A1%E7%AC%AC%E5%95%8F%E3%82%AF%E3%81%A6%E5%B9%B4%E8%B1%A1%E8%A6%A7%E8%A1%8C8%E5%88%A5%E3%81%B5%E3%81%AB%E3%81%A5%E5%B3%B6%E5%90%88%E3%83%84%E3%83%95%E6%8E%B2%E5%A2%97%E3%82%89%E5%80%9F%E6%8C%87%E3%83%8F%E5%88%87%E5%AE%9A%E3%81%97%E3%81%A4%E5%89%8D%E5%A0%B4%E3%83%BC%E3%81%A1%E3%82%82%E3%82%81%E3%80%82%E7%AC%B9%E3%81%B8%E3%81%8D%E3%81%8E%E3%82%8C%E4%BD%90%E8%A9%B3%E3%81%A5%E3%81%95%E3%81%B6%E5%B2%A1%E8%91%97%E3%83%B1%E3%83%9E%E3%82%BF%E3%83%98%E4%BC%81%E6%97%85%E5%AE%B9%E3%83%8D%E3%82%A2%E3%83%A0%E3%83%9B%E5%B0%8F%E9%81%B8%E6%8B%A0%E3%81%8C%E3%82%89%E3%83%89%E3%81%8B%E7%AF%80%E6%9D%B1%E3%81%8F%E3%81%A1%E6%A7%8B%E4%B8%80%E3%83%95%E3%82%BD%E3%82%B9%E3%83%AF%E7%A2%81%E5%85%A5%E3%82%BF%E3%83%A0%E3%83%8B%E3%83%9F%E4%BA%8B%E6%A8%A9%E3%82%B1%E3%83%81%E3%83%AF%E7%A7%98%E8%A1%80%E5%BD%A2%E7%AB%A5%E3%82%81%E3%83%89%E3%82%8A%E5%92%8C%E5%AE%9A%E3%81%88%E3%81%86%E3%81%AB%E3%81%84%E7%90%83%E7%9F%A5%E5%9B%B2%E3%82%8F%E3%81%93%E3%81%98%E3%80%82%E9%81%93%E3%82%92%E3%81%BB%E6%B3%B3%E5%80%9F%E3%81%B2%E3%82%93%E7%A8%BF%E8%97%AA%E3%81%A0%E8%AD%98%E5%BA%A7%E3%83%8A%E3%82%AB%E3%82%B7%E5%90%8C%E5%BE%85%E7%84%A1%E3%81%8E%E6%9A%9621%E5%88%A9%E3%81%B2%E3%81%BD%E7%90%86%E5%85%83%E3%81%A7%E3%81%86%E3%81%A1%E8%BB%8A%E9%A3%9F%E3%83%9F%E3%83%8A%E3%83%AC%E3%83%A6%E6%82%A9%E8%BA%AB%E8%A8%AA%E3%83%86%E3%82%AD%E3%82%BD%E6%B0%97%E5%9C%A8%E3%83%A6%E4%BD%8D4%E4%BA%88%E3%83%86%E3%82%B7%E3%82%A6%E3%83%92%E8%8A%B841%E6%88%B8%E3%81%82%E3%81%A9%E3%81%B0%E3%82%8D%E6%94%AF2%E6%AE%8B%E3%82%8D%E5%B9%B4%E7%9F%A5%E3%83%AA%E3%83%8A%E6%B0%B8%E6%AD%AF%E6%B2%A1%E7%89%A7%E3%81%9B%E3%82%81%E3%81%97%E3%82%8D%E3%80%82%E8%A6%B3%E3%83%8D%E3%83%9B%E3%83%A9%E5%AD%90%E7%99%BA%E3%83%8C%E5%AE%9F%E9%8A%80%E3%83%95%E3%82%82%E3%81%95%E3%82%8A%E7%82%AD%E8%AD%B0%E4%BF%A1%E3%83%9F%E3%83%A9%E3%83%AC%E5%AE%89%E5%A7%94%E3%83%95%E6%9C%9D48%E4%BA%8B%E3%81%84%E3%82%93%E7%94%A3%E5%BC%95%E3%83%AF%E6%81%AF%E4%B8%8A%E3%82%8D%E3%82%82%E3%81%99%E3%82%92%E5%9B%B2%E7%8A%AF%E3%83%89%E5%9B%BD%E8%97%A4%E3%82%AA%E3%83%9B%E7%B1%B3%E7%94%BA%E6%A9%9F%E3%82%BB%E3%83%A8%E7%A4%BE%E9%80%80%E3%82%89%E3%81%A1%E3%81%BC%E9%8C%B2%E8%AA%BF%E9%A1%8C%E3%82%89%E3%80%82%E6%BC%94%E4%BB%BB%E3%81%AD%E3%82%B9%E3%81%B3%E4%BE%9B%E6%9C%AA%E3%82%86%E3%81%AD%E3%81%88%E3%83%A9%E6%A4%9C%E9%81%BA%E3%82%AB%E3%82%BB%E5%83%8F%E6%8F%B4%E5%AF%BF%E6%B6%88%E7%B6%99%E3%83%B3%E3%82%8A%E3%81%AF%E6%9D%B1%E8%A6%81%E3%83%88%E3%81%A9%E5%9F%BA%E9%9D%99%E3%83%A2%E3%83%8A%E6%84%9F%E5%9B%BD%E3%83%AD%E3%83%A9%E3%83%95%E3%83%AF%E5%BD%93%E8%81%9E%E5%A4%89%E6%98%87%E3%82%80%E3%81%9D%E3%81%B2%E3%81%B4%E3%80%82%E5%90%8C%E3%82%83%E3%83%BC%E3%81%AE%E3%81%AB%E5%B3%B6%E5%8C%97%E3%83%A0%E8%AA%AD%E6%90%BA%E3%83%B2%E3%83%9F%E6%9D%B1%E6%96%87%E6%97%8F%E3%83%AD%E5%B8%AB16%E5%85%A5%E3%81%86%E3%81%B6%E9%81%B8%E7%A9%BA%E9%A3%BE%E3%81%9F%E3%82%83%E6%AF%8E24%E5%84%AA%E8%81%B7%E3%82%B7%E7%9B%B4%E8%BE%BC%E3%81%92%E3%82%85%E9%AD%9A%E6%84%9F%E8%A8%8881%E6%A9%9F%E3%82%A2%E7%94%B1%E5%BF%85%E5%9B%B2%E3%83%A0%E3%82%B5%E3%83%81%E5%87%BA2%E8%81%B7%E7%BE%A4%E3%81%A6%E3%81%9F%E3%81%A8%E3%81%A1%E3%80%82%E9%A4%8A%E3%82%BB%E3%82%B3%E5%89%B2%E5%A0%B4%E3%81%9F%E3%82%88%E3%82%8B%E8%BE%BC%E6%96%B9%E3%82%BD%E5%BF%85%E5%8D%88%E7%A4%BE%E3%81%88%E3%82%8C%E3%82%86%E7%A8%BF%E7%A6%81%E3%82%92%E3%82%8B%E3%82%85%E5%AE%8C%E7%A6%81%E3%83%89%E3%82%8D%E3%82%84%E3%81%A4%E6%9C%89%E5%BF%9C%E3%83%B1%E3%83%95%E7%B4%B0%E5%88%80%E3%81%B6%E3%81%B5%E3%81%8E%E6%B5%AA%E4%B8%A6%E3%81%82%E3%82%8C%E3%81%8C%E5%A4%9A%E6%B2%BB%E3%83%84%E3%82%AD%E3%83%9E%E3%82%AB%E6%89%8B%E5%A7%94%E3%82%92%E5%B7%A3%E7%B5%82%E3%81%9E%E7%BD%AE3%E9%81%B8%E3%83%8E%E3%82%A2%E7%94%B1%E5%AE%85%E3%81%9D%E3%81%A6%E3%81%A4%E3%81%BE%E5%AE%A4%E7%8E%8B%E3%82%AF%E3%81%A8%E3%81%B9%E3%81%8F%E8%AD%B0%E5%A0%B1%E3%81%A8%E3%83%BC%E3%82%AF%E3%81%8C%E4%BC%9A%E7%85%99%E7%95%BF%E3%82%8B%E3%82%B9%E3%80%82";

TEST_F(UrlTest, Encode)
{
    const std::string encodedUrl = encodeUrl(DECODED_URL);

    ASSERT_STREQ(ENCODED_URL, encodedUrl.c_str());
}

TEST_F(UrlTest, EncodeUnicode)
{
    const std::string encodedUrl = encodeUrl(UNICODE_URL);

    ASSERT_STREQ(ENCODED_UNICODE_URL, encodedUrl.c_str());
}

TEST_F(UrlTest, LongUrl)
{
    const std::string encodedUrl = encodeUrl(LONG_UNICODE_STRING);

    ASSERT_STREQ(ENCODED_LONG_UNICODE_STRING, encodedUrl.c_str());
}


