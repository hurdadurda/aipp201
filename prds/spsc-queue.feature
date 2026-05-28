# SPSC Queue Behavior-Driven Development Specification

Feature: Lock-free SPSC queue for inter-thread communication
  As a systems programmer
  I want a lock-free SPSC queue
  So that I can communicate between threads with minimal overhead

  Scenario: Create a queue with specified capacity
    Given I create a queue with capacity 10
    Then the queue should have capacity 10
    And the queue should be empty

  Scenario: Enqueue a single element to an empty queue
    Given an empty queue with capacity 10
    When I enqueue the value 42
    Then the enqueue should succeed
    And the queue should not be empty
    And the queue should not be full

  Scenario: Dequeue a single element from a queue with one element
    Given a queue with capacity 10 containing one element with value 42
    When I dequeue
    Then the dequeue should return the element with value 42
    And the queue should be empty

  Scenario: Enqueue and dequeue multiple elements in order
    Given an empty queue with capacity 10
    When I enqueue values 1, 2, 3, 4, 5
    And I dequeue 5 times
    Then the dequeued values should be 1, 2, 3, 4, 5 in that order
    And the queue should be empty


Feature: Query queue state (empty, full, capacity)
  As a library user
  I want to query whether the queue is empty or full
  So that I can handle overflow conditions in my application logic

  Scenario: Query capacity of an empty queue
    Given an empty queue with capacity 25
    Then the capacity should be 25

  Scenario: Check if newly-created queue is empty
    Given an empty queue with capacity 10
    Then is_empty should return true
    And is_full should return false

  Scenario: Check if queue is full when all slots are occupied
    Given an empty queue with capacity 3
    When I enqueue values 1, 2, 3
    Then is_full should return true
    And is_empty should return false

  Scenario: Check partial fill state
    Given an empty queue with capacity 10
    When I enqueue values 1, 2, 3
    Then is_empty should return false
    And is_full should return false
    And the queue should have 3 elements enqueued

  Scenario: Query state after dequeuing some elements
    Given a queue with capacity 10 containing elements 1, 2, 3, 4, 5
    When I dequeue 2 elements
    Then is_empty should return false
    And is_full should return false


Feature: Handle overflow when queue is full
  As a library user
  I want enqueue to fail gracefully when the queue is full
  So that I can handle backpressure without exceptions

  Scenario: Enqueue to a full queue returns false
    Given an empty queue with capacity 2
    And I have enqueued values 1, 2
    When I try to enqueue value 3
    Then the enqueue should fail and return false
    And the queue should still be full
    And the original value 3 should not be consumed

  Scenario: After failed enqueue, original value remains usable
    Given an empty queue with capacity 1
    And I have enqueued value 42
    When I try to enqueue value 99 and the queue is full
    Then the enqueue should fail
    And I should still be able to use value 99 in application logic

  Scenario Outline: Enqueue to queue at various fill levels
    Given an empty queue with capacity <capacity>
    And I have enqueued <filled> elements
    When I try to enqueue one more element
    Then the enqueue should <result>

    Examples:
      | capacity | filled | result |
      | 5        | 4      | succeed   |
      | 5        | 5      | fail      |
      | 2        | 1      | succeed   |
      | 2        | 2      | fail      |


Feature: Handle starvation when queue is empty
  As a library user
  I want dequeue to fail gracefully when the queue is empty
  So that I can handle starvation without exceptions

  Scenario: Dequeue from an empty queue returns nullopt
    Given an empty queue with capacity 10
    When I dequeue
    Then the dequeue should fail and return nullopt
    And the queue should still be empty

  Scenario: Dequeue becomes available after enqueue
    Given an empty queue with capacity 10
    When I enqueue value 7
    And I dequeue
    Then the dequeue should return the value 7
    And the queue should be empty

  Scenario Outline: Dequeue behavior at various fill levels
    Given an empty queue with capacity <capacity>
    And I have enqueued <count> elements
    When I dequeue <attempts> times
    Then I should receive <successes> successful values
    And the remaining failures should return nullopt

    Examples:
      | capacity | count | attempts | successes |
      | 5        | 3     | 3        | 3         |
      | 5        | 3     | 5        | 3         |
      | 10       | 0     | 1        | 0         |


Feature: Strong exception safety on enqueue
  As a library user
  I want the queue to provide strong exception safety on enqueue
  So that if element construction fails, my element is not consumed

  Scenario: If element copy throws, queue state is unchanged
    Given an empty queue with capacity 5
    And a custom element type whose copy constructor throws
    When I try to enqueue that element
    Then the enqueue should fail with an exception
    And the queue should still be empty
    And the element should still be in my possession to retry or handle

  Scenario: Subsequent enqueue succeeds after a failed enqueue
    Given an empty queue with capacity 5
    And a custom element type whose copy constructor sometimes throws
    When I try to enqueue an element that will fail
    And then I try to enqueue a valid element
    Then the first enqueue should fail
    And the second enqueue should succeed
    And only the second element should be in the queue


Feature: Query queue state from either producer or consumer thread
  As a library user
  I want to query queue state (empty/full/capacity) from either producer or consumer thread
  So that I can reason about queue status without synchronization penalties

  Scenario: Producer thread can query is_empty
    Given a queue with a producer thread
    When the producer thread queries is_empty
    Then the query should succeed without blocking

  Scenario: Consumer thread can query is_empty
    Given a queue with a consumer thread
    When the consumer thread queries is_empty
    Then the query should succeed without blocking

  Scenario: Both threads can query capacity simultaneously
    Given a queue with both producer and consumer threads
    When both threads query capacity
    Then both should receive the same value
    And neither should block or contend

  Scenario: Query capacity after several operations
    Given a queue with capacity 100
    After multiple enqueues and dequeues from different threads
    When both threads query capacity
    Then the capacity should still be 100


Feature: Allow producer thread handoff
  As a developer switching between producer threads
  I want the queue to allow thread handoff
  So that different threads can produce to the same queue over time

  Scenario: Different threads can produce sequentially
    Given an empty queue with capacity 10
    When thread A enqueues value 1
    And thread B enqueues value 2
    Then both enqueues should succeed
    And the queue should contain values 1, 2

  Scenario: Thread B can produce after thread A stops
    Given an empty queue with capacity 10
    And thread A has enqueued some values
    When thread A stops producing
    And thread B starts producing
    Then thread B should be able to enqueue new values

  # Note: Detecting concurrent producers is a debug-only feature
  # (covered separately in "Detect concurrent access" feature)


Feature: Allow consumer thread handoff
  As a developer switching between consumer threads
  I want the queue to allow thread handoff
  So that different threads can consume from the same queue over time

  Scenario: Different threads can consume sequentially
    Given a queue with capacity 10 containing values 1, 2, 3
    When thread A dequeues 2 elements
    And thread B dequeues the remaining element
    Then thread A should get values 1, 2
    And thread B should get value 3

  Scenario: Thread B can consume after thread A stops
    Given a queue with enqueued values
    And thread A has dequeued some values
    When thread A stops consuming
    And thread B starts consuming
    Then thread B should be able to dequeue the remaining values


Feature: Detect concurrent access from multiple producers (debug mode)
  As a developer
  I want the queue to detect concurrent access from multiple producers in debug builds
  So that I can catch threading bugs early

  Scenario: Debug build asserts when two producers call enqueue simultaneously
    Given a queue built in debug mode
    And two producer threads
    When both threads try to call enqueue at the same time
    Then an assertion should fire
    And the program should abort or throw an exception

  Scenario: Debug build allows thread handoff (sequential producers)
    Given a queue built in debug mode
    And two producer threads
    When thread A enqueues a value
    And thread A completes
    And thread B enqueues a value
    Then both enqueues should succeed without assertion failure


Feature: Detect concurrent access from multiple consumers (debug mode)
  As a developer
  I want the queue to detect concurrent access from multiple consumers in debug builds
  So that I can catch threading bugs early

  Scenario: Debug build asserts when two consumers call dequeue simultaneously
    Given a queue built in debug mode
    And two consumer threads
    And the queue contains some elements
    When both threads try to call dequeue at the same time
    Then an assertion should fire
    And the program should abort or throw an exception

  Scenario: Debug build allows thread handoff (sequential consumers)
    Given a queue built in debug mode
    And two consumer threads
    And the queue contains elements 1, 2
    When thread A dequeues one element
    And thread A completes
    And thread B dequeues one element
    Then both dequeues should succeed without assertion failure


Feature: Handle large element types efficiently
  As a library user
  I want the queue to handle large element types efficiently
  So that I am not forced to store only pointers

  Scenario: Enqueue and dequeue a large struct
    Given an empty queue with capacity 10
    And a large struct element (e.g., 1024 bytes)
    When I enqueue the struct
    And I dequeue it
    Then the dequeued struct should be identical to the original

  Scenario: Large elements do not bloat queue overhead
    Given a queue with capacity 100 and element type of 2048 bytes
    Then the queue should allocate approximately capacity * element_size bytes
    And the overhead (padding, atomics, etc.) should be minimal


Feature: Type safety at the API boundary
  As a library user
  I want strong type safety at the API boundary
  So that I cannot accidentally pass incorrect types to the queue

  Scenario: Capacity parameter must be a Capacity strong type
    Given the queue API
    When I try to pass an integer instead of a Capacity to the constructor
    Then the code should not compile
    And I should be forced to use the Capacity strong type

  Scenario: Template parameter determines element type
    Given a SpscQueue<int>
    When I try to enqueue a string
    Then the code should not compile
    And only int values are accepted


Feature: Non-copyable and non-movable queue
  As a library maintainer
  I want the queue to be non-copyable and non-movable
  So that it is tied to specific producer and consumer threads

  Scenario: Cannot copy a queue
    Given a SpscQueue instance
    When I try to copy the queue
    Then the code should not compile
    And the copy constructor should be deleted

  Scenario: Cannot move a queue
    Given a SpscQueue instance
    When I try to move the queue to another variable
    Then the code should not compile
    And the move constructor should be deleted


Feature: Multi-threaded producer-consumer interaction
  As a developer
  I want to verify real producer-consumer workloads
  So that I can trust the queue in concurrent scenarios

  Scenario: Producer and consumer run concurrently
    Given an empty queue with capacity 20
    And a producer thread that enqueues 100 items
    And a consumer thread that dequeues items as they become available
    When both threads run concurrently
    Then the consumer should eventually dequeue all 100 items
    And the items should be dequeued in the order they were enqueued
    And the queue should be empty when both threads finish

  Scenario: High-volume producer-consumer stress test
    Given an empty queue with capacity 10
    And a producer thread that rapidly enqueues 1000 items
    And a consumer thread that rapidly dequeues items
    When both threads run concurrently at full speed
    Then no items should be lost
    And no data races should be detected (verified with ThreadSanitizer)
    And the queue should be empty when complete

  Scenario: Producer waits for consumer when queue is full
    Given an empty queue with capacity 5
    And a producer thread that enqueues items continuously
    And a consumer thread that dequeues slowly
    When the producer fills the queue
    Then the producer's subsequent enqueue should fail (non-blocking)
    And the producer can retry when space becomes available

  Scenario: Consumer waits for producer when queue is empty
    Given an empty queue with capacity 10
    And a consumer thread that tries to dequeue immediately
    When the queue is empty
    Then the dequeue should return nullopt
    And the consumer can retry later when items are available


Feature: Property-based invariants
  As a developer
  I want property-based tests that verify invariants
  So that I catch subtle race conditions

  Scenario Outline: Invariant - total enqueued equals total dequeued at end
    Given an empty queue with capacity <capacity>
    And a random sequence of enqueue/dequeue operations
    When the sequence completes
    Then total_enqueued_successfully should equal total_dequeued_successfully

    Examples:
      | capacity |
      | 5        |
      | 10       |
      | 100      |

  Scenario: Invariant - queue never contains more than capacity elements
    Given an empty queue with capacity 10
    When concurrent producer and consumer threads operate on it
    Then at no point should the queue contain more than 10 elements

  Scenario: Invariant - dequeued elements are in enqueue order (FIFO)
    Given an empty queue with capacity 50
    When a producer enqueues elements in sequence and a consumer dequeues them
    Then the dequeued elements should maintain exact FIFO order
    And no element should be lost or duplicated
