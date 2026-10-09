# C-Dogs SDL para PlayStation 2

Port experimental, isolado em `platform/ps2`. O build desktop continua usando
SDL2_mixer e pode compilar o editor normalmente. O build PS2 gera apenas
`cdogs-sdl.elf`, sem editor, OpenGL, ENet ou dependência de SDL2_mixer.

Há duas variantes:

| Variante | Saída de áudio | Assets |
| --- | --- | --- |
| `OFF` (padrão) | desativada | pacote menor, sem arquivos de áudio |
| `RFAUDS2` | PCM estéreo S16, 48 kHz, RFAuds2 no EE/IOP | conversão no PC com FFmpeg |

**Os dois ELFs foram compilados com PS2SDK. Um teste no NetherSX2 confirmou
boot da ISO, acesso CDFS, início de vídeo/PAD e criação de configuração no
memory card, mas o menu e o SPU2 ainda não foram confirmados.** Não há BIOS
nem emulador no ambiente em que este port foi preparado.
O teste da v3 passou a abrir os PNGs, mas esgotou a RAM durante a carga gráfica
e o jogo encerrou, retornando à BIOS. Esta revisão libera as imagens temporárias
e adiciona diagnóstico do heap EE; ainda requer confirmação do menu no console.

## Build reproduzível no Linux

O bootstrap automático usa Linux x86_64, Ubuntu 24.04 ou um ambiente compatível
com os binários do ps2dev. Requer Python 3.11+, Git, Make, compilador C nativo,
CMake 3.19+ e ferramentas básicas de build. Exemplo de preparação no Ubuntu:

```sh
sudo apt-get install build-essential git python3 cmake ninja-build ffmpeg
git clone --branch ps2-port https://github.com/ReyFxck/cdogs-sdl-ps2.git
cd cdogs-sdl-ps2
python3 platform/ps2/bootstrap.py
. .ps2deps/env.sh
python3 platform/ps2/build.py
python3 platform/ps2/stage.py --zip out/cdogs-sdl-ps2-silent.zip
```

Saídas: `out/ps2/cdogs-sdl.elf`, mapa de link em `out/ps2/cdogs-sdl.map` e
`out/package/`, com o ELF e as pastas de assets lado a lado.
O ZIP contém uma pasta `cdogs-sdl/` pronta para extração.

Para gerar também a ISO de boot (recomendada no Android), instale a dependência
do empacotador em um ambiente Python separado:

```sh
sudo apt-get install python3-venv
python3 -m venv .ps2iso
.ps2iso/bin/pip install -r platform/ps2/requirements-iso.txt
.ps2iso/bin/python platform/ps2/disc.py \
  --package out/package --output out/cdogs-sdl-ps2-silent.iso
# Ou faça stage + ZIP + ISO em uma única chamada:
# .ps2iso/bin/python platform/ps2/stage.py \
#   --zip out/cdogs-sdl-ps2-silent.zip --iso out/cdogs-sdl-ps2-silent.iso
```

O empacotador escreve `SYSTEM.CNF` e `CDOGS.ELF` na raiz ISO9660 para o boot.
Os assets ficam em Joliet, preservando maiúsculas/minúsculas, espaços e nomes
maiores que 8.3. O CDFS do PS2SDK usado neste port lê essa árvore. O script
verifica o ELF de boot e o hash de **cada arquivo** extraído da imagem, além
dos limites de nomes/profundidade aceitos pelo driver. Timestamps da imagem
são fixos (ou definidos por `SOURCE_DATE_EPOCH`) e não alteram os assets.

`deps.lock.json` fixa os commits do SDL 2.32.10, PS2SDK (fontes de imports do IOP)
e RFAuds2, e o SHA256 do bundle oficial ps2dev testado (EE GCC 15.2.0).
O endereço do bundle oficial usa a release móvel `latest`: se ela mudar, o
bootstrap **interrompe** por divergência do hash. Nesse caso, forneça uma cópia
do arquivo original com `bootstrap.py --archive /caminho/ps2dev-ubuntu-latest.tar.gz`
ou atualize o lock e valide o port novamente. O lock assegura versões verificáveis;
não se promete igualdade binária entre máquinas, caminhos ou versões de CMake.

O bootstrap não altera um checkout de dependência com modificações locais.
Ele instala o toolchain em `.ps2dev/`, as dependências em `.ps2deps/`, recompila
o SDL com `SDL_AUDIO=OFF` e compila o EE/IRX da RFAuds2. Para outros diretórios:

```sh
python3 platform/ps2/bootstrap.py --ps2dev /opt/ps2dev --deps /opt/cdogs-ps2-deps
. /opt/cdogs-ps2-deps/env.sh
python3 platform/ps2/build.py --jobs 4
```

## PS2SDK já instalado

Use o arquivo de toolchain oficial `$PS2DEV/share/ps2dev.cmake`. O SDL2 dos
ports deve estar em `$PS2SDK/ports`, estático, com o renderer PS2 e o renderer
software habilitados, e **compilado com `SDL_AUDIO=OFF`**. Isso evita que o
SDL2main inicialize o driver audsrv mesmo quando o jogo não abre áudio.
O bootstrap acima também pode recompilar o SDL nessa instalação existente.

```sh
export PS2DEV=/opt/ps2dev
export PS2SDK="$PS2DEV/ps2sdk"
export GSKIT="$PS2DEV/gsKit"
export PATH="$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/bin:$PATH"
python3 platform/ps2/build.py --audio OFF
```

Para CMake direto, mantendo a mesma configuração:

```sh
cmake -S . -B out/ps2 \
  -DCMAKE_TOOLCHAIN_FILE="$PS2DEV/share/ps2dev.cmake" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release -DCDOGS_PS2=ON -DCDOGS_PS2_AUDIO=OFF
cmake --build out/ps2 --parallel 4
```

## Áudio RFAuds2

A revisão experimental testada está fixa no lock. Para usar seu checkout mais
recente, compile-o com o mesmo PS2SDK e informe `RFAUDS2_ROOT`; nesse caso a
reprodutibilidade depende também da revisão escolhida por você.

```sh
. .ps2deps/env.sh
# O bootstrap já compilou a revisão fixa. Para um checkout próprio:
# export RFAUDS2_ROOT=/caminho/RFAuds2
# export PS2SDKSRC=/caminho/fontes/ps2sdk
# make -C "$RFAUDS2_ROOT" -j4 check
python3 platform/ps2/build.py --audio RFAUDS2
python3 platform/ps2/stage.py \
  --elf out/ps2-rfauds2/cdogs-sdl.elf --output out/package-rfauds2 \
  --audio --rfa-root "$RFAUDS2_ROOT" --zip out/cdogs-sdl-ps2-rfauds2.zip
# Com o ambiente Python da seção anterior:
.ps2iso/bin/python platform/ps2/disc.py --package out/package-rfauds2 \
  --output out/cdogs-sdl-ps2-rfauds2.iso
```

O IRX fica embutido no ELF; não é necessário copiar um módulo separado.
`build.py` verifica a ausência de símbolos audsrv no ELF.
O frontend `Mix_*` específico do PS2 adapta as chamadas existentes, mas não
linka SDL2_mixer nem usa um dispositivo de áudio SDL.

- 128 canais máximos; volumes, pausa, repetição, panning/distância e o efeito
  de abafamento usado pelo jogo.
- SFX decodificados carregados sob demanda, com cache LRU de até 4 MiB;
  uma amostra maior que o cache é recusada.
- Música lida em blocos de PCM diretamente do arquivo, sem carregar a faixa
  inteira na RAM.
- Envio assíncrono com um único chamador EE. O frontend conserva o PCM que
  não foi aceito pelo IOP, inclusive quando a resposta aceita zero frames.
- Mistura e envio no loop do jogo, sem thread adicional; frames lentos podem
  causar underrun e precisam ser medidos no PS2.

O empacotador usa FFmpeg no **PC** para WAV/OGG/MP3 e IT/MOD/S3M/XM.
WAV é convertido no próprio caminho de destino. Os demais formatos recebem
um arquivo RIFF PCM `nome.ext.pcm` e um marcador vazio `nome.ext`, preservando
a enumeração de nomes do jogo. Nunca execute o empacotador sobre os assets
originais. Uma falha de decodificação interrompe o processo e informa o arquivo;
a disponibilidade de decoders de módulos depende do build do FFmpeg.

Esse fluxo dispensa decoders comprimidos no console. Música comprimida em
memória e música AdLib dos imports Wolf3D ainda não são implementadas.
Campanhas adicionais devem ser empacotadas da mesma forma. PCM ocupa muito
mais espaço em disco; o manifest informa o total convertido e hashes dos assets.

## Boot e caminhos de arquivos

Para o primeiro teste, use a variante silenciosa.

### NetherSX2/AetherSX2 no Android

Use `cdogs-sdl-ps2-silent.iso` **como imagem de jogo**, sem extrair a ISO e sem
iniciar o ELF separado. Coloque a imagem na pasta de jogos autorizada pelo
emulador, atualize a lista e abra-a como qualquer outro jogo. Os assets são
lidos por `cdfs:/` de dentro da imagem; não dependem de acesso HostFS a pastas
do Android. Para testar áudio depois, use a ISO RFAuds2 da mesma forma.

O log de um teste anterior confirmou que o ELF iniciou PS2SDK/IOP, mas encerrou
com `assets missing` antes do vídeo: `argv[0]` era um URI Android
`host:content://...`, e o HostFS recusou os caminhos de assets. URIs SAF são
nomes opacos do loader, não caminhos que o jogo ou a libc do PS2 possam abrir.
O port agora evita normalizá-los como caminhos POSIX e procura o disco CDFS.
A ISO também elimina a necessidade de manter `data/` e `graphics/` externas.
Um teste posterior confirmou boot do ELF pelo `SYSTEM.CNF`, acesso `cdfs:/`,
início do vídeo e criação de `mc0:/CDOGS/`, mas a SDL recusou imagens porque
seu `SDL_RWFromFile` exige `fstat` com tipo POSIX regular/FIFO. O `getstat`
legado do CDFS não fornecia esse tipo. O port agora adapta **só leituras CDFS**:
abre com `fopen` e cria o stream com `SDL_RWFromFP`, mantendo leitura, seek e
fechamento SDL e sem alterar o SDK/SDL. Os demais dispositivos mantêm suas
checagens normais. A falha foi reproduzida com a SDL nativa real em um teste
que injeta o mode inválido; o adaptador decodificou os pixels da fonte.
O PS2 também encerra com diagnóstico se a fonte obrigatória não carregar,
em vez de tentar desenhar glifos nulos.

O próximo log (v3) confirmou a leitura da fonte e dos PNGs, mas registrou
`outofmem!` durante a carga gráfica, falhas de alocação de controle/JSON e saída
para `rom0:OSDSYS`. A tela vermelha nesse caso apareceu **depois** que o jogo
encerrou; não foi falha de reconhecimento da ISO. O loader mantinha cada PNG
decodificado após copiar seus pixels para sprites/texturas, acumulando cerca de
9,7 MB. Agora libera essa superfície temporária e a imagem da fonte. O teste de
carga repetida também revelou e corrigiu ownership das listas de estilos, dos
mapas gráficos e das chaves antigas no rehash. São correções de liberação de
memória, sem mudar os assets, o renderer ou as opções desktop.

O teste nativo carrega os 1.695 PNGs, 5.672 sprites e a fonte três vezes com
alocação limitada a 24 MiB para o loader e SDL real. O pico verificado foi de
22.955.672 bytes; após encerramento da SDL e do seu TLS, as alocações rastreadas
retornaram a zero. Isso não representa uma medição do PS2: ponteiros, allocator,
IOP, outras estruturas e campanhas podem mudar o consumo. O novo ELF escreve
`PS2: heap video ready`, `graphics loaded` e `main menu`, com bytes usados/livres
do heap real do EE, para diagnosticar o próximo teste sem confundir uma saída
do jogo com um disco inválido. **O menu desta revisão ainda precisa de
confirmação no emulador.** Não se comprova desempenho, controle ou SPU2.

### ELF com HostFS no PC ou USB

Extraia o ZIP inteiro e
inicie o ELF **dentro da pasta extraída**, junto de `data/`, `graphics/`,
`missions/`, `dogfights/`, `music/` e `sounds/`.

No PCSX2, habilite o suporte a `host:`/HostFS. Ao iniciar um ELF, a pasta dele
serve como raiz host. Carregue o ELF extraído pela opção de iniciar um ELF do emulador. Os nomes e
a localização dessas opções variam conforme a versão. Observe o log para:

```text
PS2: C-Dogs SDL; data=host:...; network=offline
```

Se o log disser `assets missing`, confirme que `data/guns.json` e
`graphics/font.png` podem ser acessados pela raiz host. Não basta copiar o ELF.
No USB, copie a pasta completa para `mass:/cdogs-sdl/` ou `mass0:/cdogs-sdl/`
e inicie com um loader de homebrew que suporte esse dispositivo.

O SDL2main do port inicializa IOP e drivers de filesystem. O jogo procura os
assets, nesta ordem: `CDOGS_DATA_DIR`, pasta de `argv[0]` (ou `cdfs:/` para
boot CD/DVD, `host:` para um URI SAF), diretório atual, `host:`, `cdfs:/`,
`cdfs:/cdogs-sdl`, `mass:/cdogs-sdl` e `mass0:/cdogs-sdl`.
Os caminhos `host:`, `mass:`, `mc0:` e `cdfs:` são preservados pelo resolvedor.
A configuração é gravada em `config/` junto aos assets; para CD/DVD, usa
`mc0:/CDOGS/`. Loaders que fornecem variáveis de ambiente podem definir
`CDOGS_DATA_DIR` e `CDOGS_CONFIG_DIR`.
As raízes são confirmadas pela leitura real de `data/guns.json` e
`graphics/font.png`, não por `stat`. Um wrapper de tinydir local à plataforma
usa o tipo vindo de `dread` para o CDFS: o `getstat` legado dessa revisão do
PS2SDK retorna `1` para arquivos encontrados, impedindo a conversão de mode
no IOMANX, e `0` para ausentes. Não foi necessário alterar SDK, SDL ou tinydir
upstream. Os outros dispositivos continuam usando a classificação normal.

Vídeo: composição software SDL em 320×240, seguida de upload de um framebuffer
para o renderer PS2/gsKit. O driver acelerado PS2 não implementa o render target
usado pelo jogo; essa composição conserva o pipeline original sem renderer
nativo novo. O tamanho fica fixo e as opções gráficas desktop são ocultadas.
O loader agora libera PNGs temporários; o consumo dinâmico de texturas/campanhas
ainda deve ser medido nos 32 MiB do EE pelas linhas de diagnóstico do heap.

Controle: mapeamento explícito do PAD da SDL para SDL_GameController, usando o
GUID real do dispositivo. Cruz confirma/A, círculo volta/B, quadrado X,
triângulo Y, Start, Select, D-pad, sticks e ombros. Ajuste os pads no PCSX2 antes
do boot. Menu e gameplay com DualShock ainda requerem teste no emulador/hardware.

## Validação e pendências

Validação do port: compilação/link completos das variantes OFF e RFAUDS2;
ELFs MIPS n32 estáticos; ausência de audsrv; RFAuds2 compilada para EE/IOP e
seus testes nativos de mixing/resampling e transporte assíncrono aprovados.
O build desktop do jogo e editor também foi verificado com os 12 testes
upstream. A CI em `.github/workflows/ps2.yml` recompila as duas variantes.

Os testes locais da camada PS2 podem ser executados com SDL2 nativa:

```sh
# Requer SDL2 development e pkg-config no host, além do checkout RFAuds2.
python3 platform/ps2/tests/run.py --rfa-root "$RFAUDS2_ROOT"
# Incluindo verificação de ISO, nomes longos, hashes e determinismo:
# .ps2iso/bin/python platform/ps2/tests/run.py --rfa-root "$RFAUDS2_ROOT" --require-iso
make -C "$RFAUDS2_ROOT" host-test
```

Pendências reais: confirmar boot/menu, velocidade e memória no PCSX2/PS2;
confirmar reprodução/underruns da RFAuds2 no SPU2; aprimorar throughput do
renderer caso necessário. Rede/LAN, editor/OpenGL e decodificação comprimida
no PS2 ficam fora deste milestone. Não se promete suporte aos imports Wolf3D
com música em memória.

## Bloqueios encontrados e tratamento

| Bloqueio | Tratamento neste port |
| --- | --- |
| CMake desktop encontra mixer/OpenGL/ENet do host | entrada CMake PS2 separada, sem essas dependências |
| Nanopb/generator no cross build | runtime vendorizado e mensagens C já geradas no repositório |
| SDL PS2 sem render-to-texture funcional | composição software + apresentação acelerada SDL |
| PAD sem mapping adequado | mapping SDL_GameController dos índices reais do driver |
| Caminhos POSIX/storefront no console | raízes por dispositivo e shim de descoberta Steam |
| URI Android SAF em `argv[0]`/cwd e HostFS recusado | não tratar URI como caminho; imagem ISO com assets via CDFS |
| `getstat` legado CDFS classifica arquivos incorretamente | leitura real para raiz e tipo `dread` no wrapper tinydir PS2 |
| SDL rejeita PNG CDFS no filtro `fstat` de `SDL_RWFromFile` | wrapper read-only CDFS via `fopen` + `SDL_RWFromFP`, regressão com SDL real |
| PNGs temporários retidos esgotam a RAM durante a carga | liberação após cópia, regressão de carga/recarga em 24 MiB e diagnóstico do heap EE |
| `dirname`/`basename` ausentes na libc | helpers locais, incluindo raízes de dispositivos |
| GCC n32/R5900 falha com structs de campos `double` | tipo `cdogs_real_t` é float só no PS2, double no desktop |
| SDL_mixer/formatos incompatíveis com PCM | frontend silencioso ou RFAuds2 + conversão offline |
| RPC com aceitação parcial de PCM | fila pendente e reenvio apenas da cauda não aceita |
| Stack padrão pequeno para cargas recursivas | reserva de stack EE de 1 MiB no link |
| Headers de configuração disputados entre builds | headers gerados em cada diretório de build |
