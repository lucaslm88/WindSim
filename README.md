# WindSim

Simulador 2D de túnel de vento em tempo real, escrito em **C** com **raylib**, usando o método **Lattice Boltzmann (D2Q9, colisão BGK)**.

Desenhe obstáculos com o mouse e veja o escoamento do ar contornando-os, com três modos de visualização.

## Recursos

- Simulação de fluidos Lattice Boltzmann D2Q9 (grade 300 x 183 células)
- Visualização de **velocidade**, **pressão** e **vórtice** (rotacional)
- Desenho e apagamento de obstáculos em tempo real
- Ajuste da velocidade do vento e do tamanho do pincel
- Número de Reynolds aproximado no HUD
- Captura de tela (`screenshot.png`)

## Controles

| Tecla / Mouse | Ação |
|---|---|
| Botão esquerdo | Desenhar obstáculo |
| Botão direito | Apagar obstáculo |
| `Espaço` | Pausar |
| `C` | Limpar tudo |
| `1` / `2` / `3` | Modo: velocidade / pressão / vórtice |
| `+` / `-` | Aumentar / diminuir o vento |
| `[` / `]` | Tamanho do pincel |
| `S` | Salvar screenshot |

## Executar (Windows)

Baixe o `windsim.exe` na aba **Releases** deste repositório e execute.

## Compilar

Requer [raylib](https://www.raylib.com/) instalada.

```bash
# Linux
gcc src/main.c -o windsim -lraylib -lm

# Windows (MinGW)
gcc src/main.c -o windsim.exe -lraylib -lopengl32 -lgdi32 -lwinmm -lm
```
