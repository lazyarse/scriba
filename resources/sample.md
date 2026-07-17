# Scriba Sample

A quick tour of what Scriba can do.

## Typography

**Bold**, *italic*, ~~strikethrough~~, `inline code`, and a [link](#).

> Blockquote with **inline** styling.

---

## Lists

1. First
2. Second
3. Third

- Unordered
- Nested
  - Indented
  - Items
- [x] Task done
- [ ] Task pending

## Tables

| Feature | Status |
|---------|--------|
| Tables | ✓ |
| Strikethrough | ✓ |
| Task lists | ✓ |

## Code

```python
def hello():
    print("Hello, Scriba!")
```

## LaTeX Math

Inline math: $E = mc^2$, $\sum_{i=1}^{n} x_i$, $\int_0^\infty e^{-x} \, dx$

Display math:

$$
\frac{n!}{k!(n-k)!} = \binom{n}{k}
$$

$$
\mathcal{L}(\theta) = \prod_{i=1}^{n} f(x_i \mid \theta)
$$

## Admonitions

> [!note]
> Useful information you shouldn't overlook.

> [!tip]
> A helpful suggestion for a better workflow.

> [!important]
> Something critical to be aware of.

> [!warning]
> Proceed with caution here.

> [!caution]
> This could have negative consequences.

## Mermaid Diagrams

### Flowchart

```mermaid
flowchart LR
  A[Write] --> B{Preview?}
  B -->|Yes| C[Live render]
  B -->|No| D[Keep typing]
  C --> D
```

### Sequence diagram

```mermaid
sequenceDiagram
  Editor->>Parser: send markdown
  Parser->>Renderer: produce HTML
  Renderer->>Preview: set content
  Preview->>Mermaid: run diagrams
```

### State diagram

```mermaid
stateDiagram-v2
  [*] --> Idle
  Idle --> Active: input
  Active --> Idle: done
```

### Pie chart

```mermaid
pie title Time spent
  "Writing" : 22
  "CSS" : 43
  "Bugs" : 30
  "Preview" : 5
```
