<p align="center">
  <img src="assets/logo.png" alt="ClipTrace" width="380" />
</p>

# ClipTrace

Gerenciador de histórico da área de transferência para Windows, desenvolvido em C++ nativo (Win32, C++20 e WinRT). O ClipTrace monitora e cataloga textos, links, cores, arquivos e capturas de tela copiados, mantendo contexto detalhado de origem como aplicativo emissor, título da janela e URL de navegadores.

Construído para oferecer alto desempenho com consumo mínimo de memória e processamento, operando de forma totalmente local e sem dependências de runtimes pesados como Electron ou .NET.

---

## Recursos

### Captura por Tipo de Conteúdo
* **Texto e Código**: Formatação limpa e pré-visualização em tipografia monoespacada para trechos de programação.
* **Cores**: Identificação imediata de valores hexadecimais e RGB com amostragem visual da cor na listagem e na notificação.
* **Links**: Reconhecimento de endereços web com abertura direta no navegador padrão.
* **Arquivos**: Registro de nomes e caminhos de arquivos copiados pelo Explorador de Arquivos.
* **Imagens e Capturas de Tela**: Armazenamento local de imagens com suporte a visualização e abertura no visualizador padrão do Windows.

### Contexto de Origem
* Identificação do processo executável responsável pela cópia.
* Em navegadores baseados em Chromium e Edge, captura o título da aba e o endereço URL através da API Windows UI Automation.

### Notificações Nativas
* Integração com a Central de Notificações do Windows através de Windows Runtime (WinRT) Toast Notifications com AppUserModelId registrado (`palmeidev.ClipTrace`).
* Exibição de banner de imagem no topo para capturas de tela, equivalente ao padrão da Ferramenta de Captura do Windows.
* Notificações com amostra visual para cores e ícone oficial do programa para demais formatos.

### Organização e Privacidade
* **Fixação de Itens**: Fixe registros prioritários no topo do histórico para evitar exclusão acidental ou por limpeza automática.
* **Busca em Tempo Real**: Filtro instantâneo por texto, tipo de dado, título de página ou aplicativo de origem.
* **Modo Privado**: Pausa temporária do monitoramento da área de transferência com um clique.
* **Políticas de Retenção**: Limpeza programada configurável para descartar itens com mais de 2 horas, 1 dia, 1 semana, 1 mês ou desativada.
* **Persistência Local**: Histórico armazenado localmente em `%APPDATA%\ClipTrace`, sem qualquer envio de dados para a rede.

---

## Atalhos e Uso

| Ação | Atalho |
|---|---|
| Abrir / Fechar Janela | `Ctrl + Shift + V` ou `Alt + V` |
| Abrir com o atalho do Windows | `Win + V` (opcional em Configurações, enquanto o ClipTrace estiver em execução) |
| Navegar no Histórico | `Setas Cima / Baixo` |
| Copiar Selecionado | `Enter` ou clique no cartão |
| Detalhes do Registro | Clique no botão de informações (`i`) |
| Fixar / Desafixar Registro | Clique no botão de pino |
| Fechar Janela | `Esc` |

O ClipTrace permanece acessível na área de notificação da barra de tarefas (System Tray). Um clique duplo abre a janela principal e o botão direito exibe opções rápidas para pausar monitoramento, limpar histórico, abrir configurações ou encerrar o aplicativo.

## Atualizações

O ClipTrace verifica as releases publicadas do repositório ao iniciar e, enquanto estiver aberto, a cada hora. Quando existe uma versão mais recente, uma notificação oferece o download; a opção também fica disponível no menu da bandeja. O download só começa após o clique.

O atualizador exige que a release contenha `ClipTrace.exe` para x64 com digest SHA-256 e que a propriedade `ProductVersion` do executável corresponda à tag da release (sem o prefixo `v`). Depois de verificar o download, um processo auxiliar espera o ClipTrace encerrar, substitui o executável mantendo uma cópia de segurança e abre a versão nova. A pasta onde o programa está instalado precisa permitir gravação pelo usuário; se não permitir, a instalação automática informa o erro e reabre a versão anterior.

Para validar o atualizador sem instalar nada:

```powershell
MSBuild.exe tests\ClipTraceUpdatesTests.vcxproj /p:Configuration=Release /p:Platform=x64
.\tests\x64\Release\ClipTraceUpdatesTests.exe --live
.\tests\x64\Release\ClipTraceUpdatesTests.exe --install-smoke
```

---

## Requisitos de Sistema

* Windows 10 (versão 1809 ou superior) ou Windows 11 (64-bit)
* Efeitos visuais com suporte a Dark Mode e cantos arredondados nativos do Windows

---

## Compilação

O projeto foi configurado para compilação com o Visual Studio 2022 (MSVC v143 ou v145) com suporte a C++20.

### Dependências do Windows SDK
* Windows SDK (10.0.19041.0 ou superior)
* Bibliotecas vinculadas: `dwmapi.lib`, `gdi32.lib`, `shell32.lib`, `ole32.lib`, `oleaut32.lib`, `advapi32.lib`, `gdiplus.lib`, `propsys.lib`, `windowsapp.lib`

### Compilação via Linha de Comando (MSBuild)

```powershell
MSBuild.exe ClipTrace.sln /p:Configuration=Release /p:Platform=x64
```

O binário final é gerado no diretório `x64\Release\ClipTrace.exe`.

---

## Estrutura do Repositório

```
ClipTrace/
├── ClipTrace.sln             # Solução do Visual Studio
├── README.md                 # Documentação do projeto
├── .gitignore                # Arquivos e pastas ignorados pelo controle de versão
├── assets/                   # Recursos visuais do aplicativo
│   ├── ClipTrace.ico         # Ícone em múltiplos formatos e resoluções
│   ├── icon.png              # Ícone original em alta resolução
│   ├── logo.png              # Logo do programa
│   └── developer.png         # Foto do desenvolvedor
└── ClipTrace/                # Código-fonte da aplicação
    ├── ClipTrace.vcxproj     # Arquivo de projeto do Visual C++
    ├── ClipTrace.rc          # Definições de recursos (ícones e imagens embutidas)
    ├── resource.h            # IDs de recursos
    ├── icons.h               # Vetores e renderizadores de interface GDI+
    └── main.cpp              # Implementação completa da aplicação Win32
```

---

## Licença e Distribuição

Software gratuito e de código aberto para uso livre.

---

## Desenvolvedor

Desenvolvido por **Pablo Almeida**  
GitHub: [https://github.com/palmeidev/](https://github.com/palmeidev/)
