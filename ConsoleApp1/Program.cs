using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;

namespace ConsoleApp1
{
    class Employee
    {
        public string Name { get; set; }
        public int Salary { get; set; }

        public Employee(string name, int salary)
        {
            Name = name;
            Salary = salary;
        }

        public override string ToString() { return "[" + Name + ", " + Salary + "]"; }
    }

    class EmployeeList : IEnumerable<Employee>
    {
        public EmployeeList() { }
        public EmployeeList(int reserveSize) { m_employees = new Employee[reserveSize]; }
        public EmployeeList(IEnumerable<Employee> employees)
        {
            m_employees = employees.ToArray();
            Count = m_employees.Length;
        }

        public int Count { get; private set; }

        public Employee this[int i] { get { return m_employees[i]; } }

        public void Add(Employee employee)
        {
            if (Count == m_employees.Length)
                Array.Resize(ref m_employees, m_employees.Length + 1);

            m_employees[Count++] = employee;
        }

        IEnumerator IEnumerable.GetEnumerator() { return GetEnumerator(); }
        public IEnumerator<Employee> GetEnumerator()
        {
            for (int i = 0; i < Count; ++i)
                yield return m_employees[i];
        }

        public EmployeeList SelectHighestPaidEmployees(EmployeeList list2, int n1, int n2)
        {
            return new EmployeeList(this.OrderByDescending(employee => employee.Salary).Take(n1)
                .Concat(list2.OrderByDescending(employee => employee.Salary).Take(n2)));
        }

        private Employee[] m_employees = new Employee[0];
    }
    class Program
    {
        static void Main(string[] args)
        {
            var list1 = new EmployeeList(10); // reserving 10 elements
            list1.Add(new Employee("One", 1));
            list1.Add(new Employee("Two", 2));
            list1.Add(new Employee("Three", 3));
            list1.Add(new Employee("Four", 4));
            list1.Add(new Employee("Five", 5));

            var list2 = new EmployeeList(); // no reserve
            list2.Add(new Employee("Six", 6));
            list2.Add(new Employee("Seven", 7));
            list2.Add(new Employee("Eight", 8));
            list2.Add(new Employee("Nine", 9));
            list2.Add(new Employee("Ten", 10));

            // Select up to 3 employees from each of the lists
            var list3 = list1.SelectHighestPaidEmployees(list2, 3, 3);

            for (int i = 0; i < list3.Count; ++i)
                Console.WriteLine(list3[i]);

            Console.WriteLine("---------------- once again -----------------");

            foreach (var employee in list3)
                Console.WriteLine(employee);
        }
    }
}
