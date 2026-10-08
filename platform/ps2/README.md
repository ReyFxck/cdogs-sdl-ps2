# C-Dogs SDL para PlayStation 2

Port experimental, isolado em `platform/ps2`. O build desktop continua usando
SDL2_mixer e pode compilar o editor normalmente. O build PS2 gera apenas
`cdogs-sdl.elf`, sem editor, OpenGL, ENet ou dependência de SDL2_mixer.

Há duas variantes:

| Variante | Saída de áudio | Assets |
| --- | --- | --- |
| `OFF` (padrão) | desativada | pacote menor, sem arquivos de áudio |
| `RFAUDS2` | PCM estéreo S16, 48 kHz, RFAuds2 no EE/IOP | conversão no PC com FFmpeg |

**Os dois ELFs foram compilados com PS2SDK. O boot até o menu no PCSX2 e o
funcionamento do SPU2 em hardware ainda não foram confirmados.** Não há BIOS
nem emulador no ambiente em que este port foi preparado.

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

Para o primeiro teste, use a variante silenciosa. Extraia o ZIP inteiro e
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
assets, nesta ordem: `CDOGS_DATA_DIR`, pasta de `argv[0]`, diretório atual,
`host:`, `mass:/cdogs-sdl` e `mass0:/cdogs-sdl`.
Os caminhos `host:`, `mass:`, `mc0:` e `cdfs:` são preservados pelo resolvedor.
A configuração é gravada em `config/` junto aos assets; para CD/DVD, usa
`mc0:/CDOGS/`. Loaders que fornecem variáveis de ambiente podem definir
`CDOGS_DATA_DIR` e `CDOGS_CONFIG_DIR`.

Vídeo: composição software SDL em 320×240, seguida de upload de um framebuffer
para o renderer PS2/gsKit. O driver acelerado PS2 não implementa o render target
usado pelo jogo; essa composição conserva o pipeline original sem renderer
nativo novo. O tamanho fica fixo e as opções gráficas desktop são ocultadas.
O consumo dinâmico de texturas/campanhas ainda deve ser medido nos 32 MiB do EE.

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
| `dirname`/`basename` ausentes na libc | helpers locais, incluindo raízes de dispositivos |
| GCC n32/R5900 falha com structs de campos `double` | tipo `cdogs_real_t` é float só no PS2, double no desktop |
| SDL_mixer/formatos incompatíveis com PCM | frontend silencioso ou RFAuds2 + conversão offline |
| RPC com aceitação parcial de PCM | fila pendente e reenvio apenas da cauda não aceita |
| Stack padrão pequeno para cargas recursivas | reserva de stack EE de 1 MiB no link |
| Headers de configuração disputados entre builds | headers gerados em cada diretório de build |
