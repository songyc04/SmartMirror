import java.util.Scanner;
import java.util.Arrays;
import java.util.ArrayList;

class Student {
    static int targetId = 260100;

    private String name;
    private String gender;
    private int age;
    private ArrayList<Double> grades;
    private String contact;
    private final String id;

    public Student(String s_name) {
        this.name = s_name;
        this.id = Integer.toString(++targetId);
        this.grades = new ArrayList<>(Arrays.asList(0.0, 0.0, 0.0, 0.0));
        this.age = 0;
        this.gender = null;
        this.contact = null;
    }
    public Student(String s_name, int graduatedId, Student student) {
        this.name = s_name;
        this.id = Integer.toString(graduatedId);
        this.grades = student.getGrades();
        this.age = student.getAge();
        this.gender = student.getGender();
        this.contact = student.getContact();
    }
    public void setName(String s_name) {
        this.name = s_name;
    }
    public void setGender(String s_gender) {
        this.gender = s_gender;
    }
    public void setAge(int s_age) {
        this.age = s_age;
    }
    public void setContact(String s_contact) {
        this.contact = s_contact;
    }
    public void setGrades(int semester, double s_grade) {
        if (semester >= 1 && semester <= 4)
        {
            this.grades.add(semester - 1, s_grade);
        }
        else
        {
            System.err.println("잘못된 학기 입력입니다. 1 ~ 4 사이로 입력해주세요.");
        }        
    }
    public String getName() {
        return this.name;
    }
    public int getAge() {
        return this.age;
    }
    public String getContact() {
        return this.contact;
    }
    public String getId() {
        return this.id;
    }
    public String getGender() {
        return this.gender;
    }
    public ArrayList<Double> getGrades() {
        return grades;
    }
    public void getAllInfo() {
        System.out.printf("학번: %s\n", this.id);
        System.out.printf("이름: %s\n", this.name);
        System.out.printf("성별: %s\n", this.gender);
        System.out.printf("연락처: %s\n", this.contact);
        System.out.printf("나이: %d\n", this.age);
        System.out.printf("학기별 성적:\n");
        for (int i = 0; i < 4; i++)
        {
            System.out.printf("%d학기: [%.1f]\n", i + 1, grades.get(i));
        }
        System.out.println("==================================");
    }
}

class Graduated extends Student {
    private static int graduatedId = 261100;
    private String labName;
    private String advisor;

    public Graduated(String s_name, String s_labName, String s_advisor, Student student) {
        super(s_name, ++graduatedId, student);
        this.labName = s_labName;
        this.advisor = s_advisor;
    }

    public void setLabName(String s_labName) {
        this.labName = s_labName;
    }
    public void setAdvisor(String s_advisor) {
        this.advisor = s_advisor;
    }

    public void copyInfo() {

    }

    public String getLabName() {
        return this.labName;
    }
    public String getAdvisor() {
        return this.advisor;
    }


    @Override
    public void getAllInfo() {
        System.out.println("아래 학생은 대학원생입니다.");
        super.getAllInfo();
        System.out.printf("연구실: %s\n", this.labName);
        System.out.printf("지도교수: %s 교수\n", this.advisor);
    }
}

public class Project {
    public static void main(String[] args) throws Exception {
        System.out.println("=== 학생 관리 시스템 ===");
        ArrayList<Student> studentList = new ArrayList<>();
        ArrayList<Graduated> graduatedList = new ArrayList<>();
        Scanner sc = new Scanner(System.in);

        while (true)
        {
            System.out.println("원하는 작업을 선택하세요.");
            System.out.println("0) 시스템 종료 | 1) 학생 등록 | 2) 학생 세부정보 등록/수정 | 3) 선택 학생 정보 조회 | 4) 전체 학생 조회");
            String menu = sc.nextLine();
            

            if (menu.equals("0"))       // 시스템 종료
            {
                sc.close();
                System.out.println("학생 관리 시스템을 종료합니다.");
                return;
            }
            else if (menu.equals("1"))  // 학생 등록
            {
                System.out.print("일반 대학생을 등록하려면 1, 대학원생을 등록하려면 2를 입력해주세요: ");
                String mode = sc.nextLine();

                if (mode.equals("1"))
                {
                    System.out.printf("등록하시려면 학생 이름을, 등록을 취소하시려면 0을 입력해주세요: ");
                    String s_name = sc.nextLine();
                    if (s_name.equals("0"))
                    {
                        System.out.println("학생 등록을 취소합니다. 작업 선택 화면으로 이동합니다.");
                        System.out.printf("================================================\n\n");
                        continue;
                    }
                    else
                    {
                        Student stu01 = new Student(s_name);
                        System.out.printf("%s 학생이 등록되었습니다.\n", stu01.getName());
                        studentList.add(stu01);
                    }
                }
                else if (mode.equals("2"))
                {
                    if (studentList.size() == 0)
                    {
                        System.out.println("등록된 학생이 없어 대학원생을 등록할 수 없습니다. 작업 선택 화면으로 이동합니다.");
                        System.out.printf("================================================\n\n");
                    }
                    else
                    {
                        System.out.printf("등록된 학생은 %d명입니다. 대학원생으로 등록할 학생의 학번을 입력해주세요. 학번은 2601-- 형식입니다: ", studentList.size());
                        String id = sc.nextLine();
                        Student selectedStudent = null;

                        for (Student student: studentList)
                        {
                            if (id.equals(student.getId()))
                            {
                                selectedStudent = student;
                                System.out.printf("%s 학번의 학생을 찾았습니다. 대학원 정보 입력으로 이동합니다.\n", selectedStudent.getId());
                                break;
                            }
                        }

                        if (selectedStudent == null)
                        {
                            System.out.printf("%s 학번의 학생이 없습니다. 작업 선택 화면으로 이동합니다.\n", id);
                            System.out.printf("================================================\n\n");
                        }
                        else
                        {
                            System.out.print("등록할 대학원생의 연구실 이름을 입력해주세요: ");
                            String labName = sc.nextLine();
                            System.out.print("등록할 대학원생의 지도교수 성명을 입력해주세요: ");
                            String advisor = sc.nextLine();

                            Graduated graduated = new Graduated(selectedStudent.getName(), labName, advisor, selectedStudent);
                            graduatedList.add(graduated);

                            // Graduated graduated = new Graduated(selectedStudent.getName(), labName, advisor);
                            // graduatedList.add(graduated);
                            
                            graduated.getAllInfo();
                            System.out.println("대학원생이 정상적으로 등록되었습니다. 작업 선택 화면으로 이동합니다.");
                            System.out.printf("================================================\n\n");

                            for (Student student: studentList)
                            {
                                if (id.equals(student.getId()))
                                {
                                    studentList.remove(student);
                                    break;
                                }
                            }

                            continue;
                        }


                    }
                }
                else
                {
                    System.out.println("잘못된 선택 입력입니다. 작업 선택 화면으로 이동합니다.");
                    System.out.printf("================================================\n\n");
                    continue;
                }


                

            }
            else if (menu.equals("2"))  // 학생 세부정보 등록/수정
            {
                System.out.printf("등록된 학생은 총 %d명입니다. 세부정보를 수정할 학생의 학번을 입력해주세요. 학번은 2601-- 형식입니다.\n", studentList.size());
                System.out.print("===> ");
                Student selectedStudent = null;
                String selectedId = sc.nextLine();
                String selectedInfo = null;
                for (Student s: studentList)
                {
                    if (s.getId().equals(selectedId))
                    {
                        System.out.printf("%s 학번의 학생을 찾았습니다. 등록/수정할 정보를 입력해주세요. (name, gender, contact, age, grade): ", selectedId);
                        selectedStudent = s;
                        selectedInfo = sc.nextLine();
						break;
                    }
                    else
                    {
                        System.out.printf("%s 학번을 가진 학생이 없습니다. 작업 선택 화면으로 이동합니다.\n", selectedId);
                        System.out.printf("================================================\n\n");
                        continue;
                    }
                }

                if (selectedStudent == null) continue;
                else
                {
                    if (selectedInfo.equals("name"))
                    {
                        System.out.printf("등록된 이름은 %s입니다. 수정하려면 이름을, 수정하지 않으려면 0을 입력해주세요: ", selectedStudent.getName());
                        String inputName = sc.nextLine();
                        if (inputName.equals("0")) continue;
                        else selectedStudent.setName(inputName);
                    }
                    else if (selectedInfo.equals("gender"))
                    {
                        if (selectedStudent.getGender() == null)
                        {
                            System.out.print("등록된 성별이 없습니다. 성별을 입력해주세요: ");
                        }
                        else
                        {
                            System.out.printf("등록된 성별은 %s입니다. 수정하려면 성별을, 수정하지 않으려면 0을 입력해주세요: ", selectedStudent.getGender());
                        }
                        String inputGender = sc.nextLine();

                        if (inputGender.equals("0")) continue;
                        else selectedStudent.setGender(inputGender);
                    }
                    else if (selectedInfo.equals("contact"))
                    {
                        if (selectedStudent.getContact() == null)
                        {
                            System.out.print("등록된 연락처가 없습니다. 연락처를 입력해주세요: ");
                        }
                        else
                        {
                            System.out.printf("등록된 연락처는 %s입니다. 수정하려면 연락처를, 수정하지 않으려면 0을 입력해주세요: ", selectedStudent.getContact());
                        }

                        String inputContact = sc.nextLine();

                        if (inputContact.equals("0")) continue;
                        else selectedStudent.setContact(inputContact);
                    }
                    else if (selectedInfo.equals("age"))
                    {
                        if (selectedStudent.getAge() == 0)
                        {
                            System.out.print("등록된 나이가 없습니다. 나이를 입력해주세요: ");
                        }
                        else
                        {
                            System.out.printf("등록된 나이는 %d살입니다. 수정하려면 나이를, 수정하지 않으려면 0을 입력해주세요: ", selectedStudent.getAge());
                        }
                        int inputAge = sc.nextInt();
						sc.nextLine();
                        if (inputAge == 0) continue;
                        else selectedStudent.setAge(inputAge);
                    }
                    else if (selectedInfo.equals("grade"))
                    {
                        int inputSemester;
                        double grade;
                        System.out.printf("%s 학번의 학생 성적은 다음과 같습니다.\n", selectedStudent.getId());
                        for (int i = 0; i < selectedStudent.getGrades().size(); i++)
                        {
                            System.out.printf("%d학기 성적: [%.1f]\n", i + 1, selectedStudent.getGrades().get(i));
                        }

                        while (true)
                        {
                            System.out.print("성적을 수정하려면 해당 학기를, 수정을 종료하려면 0을 입력해주세요: ");
                            inputSemester = Integer.parseInt(sc.nextLine());
                            
                            switch (inputSemester)
                            {
                                case 1:
                                    System.out.print("학점을 입력해주세요: ");
                                    grade = Double.parseDouble(sc.nextLine());
                                    selectedStudent.setGrades(inputSemester, grade);
                                    break;
                                case 2:
                                    System.out.print("학점을 입력해주세요: ");
                                    grade = Double.parseDouble(sc.nextLine());
                                    selectedStudent.setGrades(inputSemester, grade);
                                    break;
                                case 3:
                                    System.out.print("학점을 입력해주세요: ");
                                    grade = Double.parseDouble(sc.nextLine());
                                    selectedStudent.setGrades(inputSemester, grade);
                                    break;
                                case 4:
                                    System.out.print("학점을 입력해주세요: ");
                                    grade = Double.parseDouble(sc.nextLine());
                                    selectedStudent.setGrades(inputSemester, grade);
                                    break;
                                default:
                                    System.out.println("성적 입력을 종료합니다.");
                                    break;
                            }

                            if (inputSemester == 0) break;
                        }    
                    }
                    else
                    {
                        System.out.println("잘못된 입력입니다. 작업 선택 화면으로 이동합니다.");
                        System.out.printf("================================================\n\n");
                        continue;
                    }
                    System.out.println(" *** 학생 정보가 업데이트되었습니다. ***");
                    selectedStudent.getAllInfo();
                }                
            }
            else if (menu.equals("3"))  // 선택 학생 정보 조회
            {
                int len = studentList.size();
                Student selectedStudent = null;
                String selectedId = null;
                if (len == 0)
                {
                    System.out.printf("등록된 학생이 없습니다. 작업 선택 화면으로 이동합니다.\n\n");
                }
                else
                {
                    System.out.printf("등록된 학생 수는 %d명입니다. 조회할 학생의 학번을 입력해주세요. 학번은 2601-- 형식입니다.\n", len);
                    System.out.print("===> ");

                    selectedId = sc.nextLine();
                    for (Student s: studentList)
                    {
                        if (s.getId().equals(selectedId)) selectedStudent = s;
                    }

                    if (selectedStudent == null)
                    {
                        System.out.printf("%s 학번으로 등록된 학생이 없습니다. 작업 선택 화면으로 이동합니다.\n", selectedId);
                        continue;
                    }
                    else
                    {
                        System.out.printf("%s 학번의 학생 정보를 조회합니다.\n", selectedId);
                        selectedStudent.getAllInfo();
                    }
                }
                
            }
            else if (menu.equals("4"))  // 전체 학생 조회
            {
                System.out.printf("전체 학생의 정보를 조회합니다. 등록된 학생은 총 %d명입니다.\n", studentList.size());
                System.out.println("=== 일반 대학생 정보 ===");
                if (studentList.size() != 0)
                {
                    for (Student s: studentList)
                    {
                        System.out.printf("*** %d번 학생 정보 ***\n", studentList.indexOf(s) + 1);
                        s.getAllInfo();
                    }
                }
                else
                {
                    System.out.println("일반 대학생이 없습니다.");
                }

                System.out.println("=== 대학원생 정보 ===");
                if (graduatedList.size() != 0)
                {
                    for (Graduated s: graduatedList)
                    {
                        System.out.printf("*** %d번 학생 정보 ***\n", graduatedList.indexOf(s) + 1);
                        s.getAllInfo();
                    }
                }
                else
                {
                    System.out.println("대학원생이 없습니다.");
                }
                
            }
            else                                 // 오입력 제어
            {
                System.out.println("잘못된 선택 입력입니다. 작업 선택 화면으로 이동합니다.");
                System.out.printf("================================================\n\n");
                continue;
            }

        }   
    }
}