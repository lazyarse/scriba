# Mermaid Diagram Examples

This file demonstrates all supported Mermaid diagram types. Scriba renders ` ```mermaid `
blocks natively in the preview pane. Open this file alongside the preview to see
each diagram rendered.

---

## Flowchart

Process flows, workflows, decision trees, and swimlanes.

```mermaid
flowchart TD
  Start([Start]) --> Auth{Authenticated?}
  Auth -->|Yes| Dashboard[Load Dashboard]
  Auth -->|No| Login[Login Page]
  Login --> Validate{Valid Credentials?}
  Validate -->|Yes| Dashboard
  Validate -->|No| Error[Show Error]
  Error --> Login
  Dashboard --> End([End])
```

---

## Sequence Diagram

Actor interactions and message flows over time.

```mermaid
sequenceDiagram
  participant U as User
  participant F as Frontend
  participant A as API
  participant D as Database

  U->>F: Enter credentials
  F->>A: POST /login
  A->>D: Query user
  D-->>A: User record
  A->>A: Verify password
  alt Success
    A-->>F: 200 JWT token
    F-->>U: Redirect
  else Failure
    A-->>F: 401 Unauthorized
    F-->>U: Show error
  end
```

---

## Class Diagram

Object-oriented class structures, interfaces, and relationships.

```mermaid
classDiagram
  class Animal {
    +String name
    +int age
    +speak() void
  }
  class Dog {
    +String breed
    +fetch() void
  }
  class Cat {
    +bool indoor
    +purr() void
  }
  Animal <|-- Dog
  Animal <|-- Cat
  class PetOwner {
    +String name
    +adopt(Animal) void
  }
  PetOwner "1" --> "*" Animal : owns
```

---

## Entity Relationship Diagram

Database entity relationships with cardinality.

```mermaid
erDiagram
  USER ||--o{ ORDER : places
  ORDER ||--|{ LINE-ITEM : contains
  ORDER ||--|| PAYMENT : has
  PRODUCT ||--o{ LINE-ITEM : includes
  USER {
    int id PK
    string email
    string name
  }
  ORDER {
    int id PK
    int userId FK
    date createdAt
    string status
  }
  LINE-ITEM {
    int id PK
    int orderId FK
    int productId FK
    int quantity
    float price
  }
  PRODUCT {
    int id PK
    string name
    float price
    int stock
  }
  PAYMENT {
    int id PK
    int orderId FK
    float amount
    string method
  }
```

---

## State Diagram

State machines with transitions and substates.

```mermaid
stateDiagram-v2
  [*] --> Idle
  Idle --> Processing : start
  Processing --> Paused : pause
  Processing --> Completed : finish
  Paused --> Processing : resume
  Paused --> Cancelled : cancel
  Completed --> [*]
  Cancelled --> [*]

  state Processing {
    [*] --> FetchData
    FetchData --> Validate
    Validate --> Transform
    Transform --> [*]
  }
```

---

## Gantt Chart

Project timelines, task dependencies, and milestones.

```mermaid
gantt
  title Project Timeline
  dateFormat  YYYY-MM-DD
  axisFormat  %b %d

  section Planning
  Requirements     :a1, 2024-06-01, 5d
  Design           :a2, after a1, 7d

  section Development
  Backend API      :b1, after a2, 14d
  Frontend         :b2, after a2, 12d
  Database         :b3, after a2, 5d

  section Testing
  Integration      :c1, after b1, 5d
  UAT              :c2, after c1, 4d

  section Deploy
  Staging          :d1, after c2, 2d
  Production       :milestone, after d1, 0d
```

---

## Pie Chart

Proportional data display.

```mermaid
pie
  title Development Stack Usage
  "TypeScript" : 40
  "Python" : 25
  "Rust" : 15
  "Go" : 10
  "Other" : 10
```

---

## User Journey

User experience maps with satisfaction scores.

```mermaid
journey
  title Coffee Shop Experience
  section Order
    Browse menu: 4: Customer
    Select drink: 5: Customer
    Pay: 3: Customer, Cashier
  section Preparation
    Wait for drink: 2: Customer
    Make drink: 4: Barista
  section Enjoy
    Receive drink: 5: Customer, Barista
    Drink coffee: 5: Customer
```

---

## Git Graph

Branch and merge strategies.

```mermaid
gitGraph
  commit id: "Initial commit"
  commit id: "Add CI/CD"
  branch develop
  checkout develop
  commit id: "Refactor auth"
  commit id: "Add tests"
  branch feature/notifications
  checkout feature/notifications
  commit id: "Email service"
  commit id: "Push notifications"
  checkout develop
  merge feature/notifications
  checkout main
  merge develop tag: "v1.0.0"
```

---

## Mind Map

Hierarchical ideas and concept maps.

```mermaid
mindmap
  root((Software))
    Frontend
      React
      TypeScript
      Tailwind
    Backend
      API Gateway
      Microservices
      Database
    DevOps
      CI/CD
      Docker
      Monitoring
    Quality
      Unit Tests
      Integration Tests
      E2E Tests
```

---

## Timeline

Chronological events and milestones.

```mermaid
timeline
  title Company Milestones
  2018 : Founded
  2019 : Seed funding
       : MVP launch
  2020 : Series A
       : 10K users
  2022 : Series B
       : International expansion
  2024 : IPO
```

---

## C4 Model

Software architecture context, containers, and components.

```mermaid
C4Context
  title System Context - Online Store

  Person(customer, "Customer", "Browses and purchases products")
  System(store, "Online Store", "E-commerce platform")
  System_Ext(payment, "Payment Gateway", "Processes payments")
  System_Ext(shipping, "Shipping Service", "Handles deliveries")

  Rel(customer, store, "Uses")
  Rel(store, payment, "Charges via")
  Rel(store, shipping, "Ships via")
```

---

## Block Diagram

System blocks with inputs and outputs to represent systems.

```mermaid
block
columns 1

  block:STEP1
    A["Input"] 
    B["Process"] 
  end
  space
  D["Output"]
  A --> D 
  B --> D
```

---

## Packet Diagram

Network packet structure visualization.

```mermaid
---
title: "HTTP Request Packet"
---
packet
  0-3: "Destination IP"
  4-7: "Source IP"
  8-9: "Destination Port"
  10-11: "Source Port"
  12-13: "Length"
  14: "Flags"
  15: "TTL"
  16-19: "Checksum"
  20-35: "Payload"
```

---

## Xychart

Line and bar charts with explicit categorical or numeric axes.

```mermaid
xychart-beta
  title "Sales by Month"
  x-axis [Jan, Feb, Mar, Apr, May]
  y-axis "Revenue" 0 --> 25000
  bar [17200, 19800, 23500, 21300, 24800]
  line [20000, 21500, 23000, 22500, 24500]
```

---

## Quadrant Chart

Positional categorization against two axes.

```mermaid
quadrantChart
  title Task Priority
  x-axis "Urgent" --> "Not Urgent"
  y-axis "Important" --> "Not Important"
  quadrant-1 Do First
  quadrant-2 Schedule
  quadrant-3 Delegate
  quadrant-4 Eliminate
  Fix outage: [0.15, 0.8]
  Ship release: [0.3, 0.7]
  Pay invoices: [0.75, 0.35]
  Reorganize pantry: [0.85, 0.9]
```

---

## Requirement Diagram

Requirement traceability and relationships.

```mermaid
requirementDiagram
  requirement LogIn as req1 {
    id: 1
    text: "Users shall be able to log in."
    risk: high
    verifymethod: test
  }
  requirement AuthedApi as req2 {
    id: 2
    text: "API calls shall require authentication."
    risk: medium
    verifymethod: test
  }
  element LoginForm as ef {
    type: component
  }
  element AuthService as es {
    type: service
  }
  req1 - contains -> ef
  req1 - traces -> req2
  req2 - satisfiedBy -> es
```

---

## Sankey

Flows of quantity between nodes.

```mermaid
sankey-beta
  Coal, Pulverized, 78
  Pulverized, Heat, 58
  Heat, Steam, 50
  Steam, Electricity, 22
  Electricity, Grid, 20
  Electricity, Self, 2
```

---

## Radar Chart

Multivariate data displayed as spokes around a central point, with one value
per axis per curve.

```mermaid
radar-beta
  title Team Skill Assessment
  axis speed["Speed"], reliability["Reliability"], comfort["Comfort"]
  curve car["Car"]{6, 9, 8}
  curve bike["Bike"]{4, 3, 6}
  showLegend true
  max 100
  min 0
  graticule circle
  ticks 5
```

---

## Architecture

Software/hardware components and the edges between them.

```mermaid
architecture-beta
  group net[Network]
  group cloud[Cloud]
  service db[Database] in cloud
  service api[API Server] in cloud
  webWeb[Web Client] in net

  edge db -> api
  edge api -> webWeb
```

---

## Styled Example

Using Mermaid configuration directives for custom theming.

```mermaid
---
config:
  theme: base
  themeVariables:
    primaryColor: "#dae8fc"
    primaryBorderColor: "#6c8ebf"
    secondaryColor: "#d5e8d4"
    tertiaryColor: "#f8cecc"
    fontFamily: "monospace"
---
flowchart LR
  subgraph Frontend
    A[React App]
  end
  subgraph Backend
    B[API Server]
    C[(Database)]
  end
  A <--> B
  B <--> C
  B -.-> D[External Service]
```
